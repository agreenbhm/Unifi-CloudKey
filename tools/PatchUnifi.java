import java.nio.file.*;
import java.util.*;
import java.util.zip.*;
import jdk.internal.org.objectweb.asm.*;
import jdk.internal.org.objectweb.asm.tree.*;

/** Patch only the launcher's 32-bit rejection; retain this package's own classes and manifest. */
public final class PatchUnifi {
    private static final String LAUNCHER = "com/ubnt/ace/Launcher.class";

    private static AbstractInsnNode next(AbstractInsnNode instruction) {
        do { instruction = instruction.getNext(); }
        while (instruction != null && instruction.getOpcode() < 0);
        return instruction;
    }

    static byte[] patch(byte[] original) {
        ClassNode type = new ClassNode();
        new ClassReader(original).accept(type, 0);
        if (!type.name.equals("com/ubnt/ace/Launcher"))
            throw new IllegalArgumentException("Unexpected launcher class");
        List<MethodInsnNode> candidates = new ArrayList<>();
        MethodNode main = null;
        for (MethodNode method : type.methods) {
            if (!method.name.equals("main") || !method.desc.equals("([Ljava/lang/String;)V")
                    || (method.access & Opcodes.ACC_STATIC) == 0) continue;
            main = method;
            for (AbstractInsnNode instruction : method.instructions) {
                if (!(instruction instanceof MethodInsnNode call)
                        || call.getOpcode() != Opcodes.INVOKESTATIC
                        || !call.desc.equals("()Z")
                        || !call.owner.startsWith("com/ubnt/service/system/")) continue;
                AbstractInsnNode following = next(call);
                if (!(following instanceof JumpInsnNode jump) || jump.getOpcode() != Opcodes.IFNE) continue;
                boolean message = false, exit = false;
                int instructions = 0;
                AbstractInsnNode cursor = following.getNext();
                for (; cursor != null && cursor != jump.label; cursor = cursor.getNext()) {
                    if (cursor.getOpcode() >= 0 && ++instructions > 12) break;
                    if (cursor instanceof LdcInsnNode literal
                            && "32-bit OS not supported!".equals(literal.cst)) message = true;
                    if (cursor instanceof MethodInsnNode invocation
                            && invocation.getOpcode() == Opcodes.INVOKESTATIC
                            && invocation.owner.equals("java/lang/System")
                            && invocation.name.equals("exit") && invocation.desc.equals("(I)V")) {
                        AbstractInsnNode previous = invocation.getPrevious();
                        while (previous != null && previous.getOpcode() < 0) previous = previous.getPrevious();
                        exit = previous != null && previous.getOpcode() == Opcodes.ICONST_M1;
                    }
                }
                if (cursor == jump.label && message && exit) candidates.add(call);
            }
        }
        if (candidates.size() != 1)
            throw new IllegalArgumentException("Expected exactly one recognized 32-bit launcher gate; found " + candidates.size());
        main.instructions.set(candidates.get(0), new InsnNode(Opcodes.ICONST_1));
        ClassWriter writer = new ClassWriter(0);
        type.accept(writer);
        return writer.toByteArray();
    }

    public static void main(String[] arguments) throws Exception {
        if (arguments.length != 2) throw new IllegalArgumentException("Usage: PatchUnifi input.jar output.jar");
        Path input = Path.of(arguments[0]), output = Path.of(arguments[1]);
        if (input.toAbsolutePath().normalize().equals(output.toAbsolutePath().normalize()))
            throw new IllegalArgumentException("Input and output must differ");
        // Validate before creating output, so unknown layouts cannot leave an apparently patched JAR.
        byte[] patched;
        try (ZipFile jar = new ZipFile(input.toFile())) {
            ZipEntry launcher = jar.getEntry(LAUNCHER);
            if (launcher == null) throw new IllegalArgumentException("Launcher missing");
            patched = patch(jar.getInputStream(launcher).readAllBytes());
            for (ZipEntry entry : Collections.list(jar.entries())) {
                String name = entry.getName().toUpperCase(Locale.ROOT);
                if (name.startsWith("META-INF/") && (name.endsWith(".SF") || name.endsWith(".RSA")
                        || name.endsWith(".DSA") || name.endsWith(".EC")))
                    throw new IllegalArgumentException("Signed JAR requires manual review");
            }
        }
        Path temporary = Files.createTempFile(output.toAbsolutePath().getParent(), "unifi-patch-", ".jar");
        try {
            try (ZipFile jar = new ZipFile(input.toFile());
                    ZipOutputStream result = new ZipOutputStream(Files.newOutputStream(temporary))) {
                for (ZipEntry entry : Collections.list(jar.entries())) {
                    ZipEntry copy = new ZipEntry(entry.getName());
                    copy.setTime(entry.getTime());
                    result.putNextEntry(copy);
                    result.write(entry.getName().equals(LAUNCHER) ? patched : jar.getInputStream(entry).readAllBytes());
                    result.closeEntry();
                }
            }
            Files.move(temporary, output, StandardCopyOption.REPLACE_EXISTING);
        } finally { Files.deleteIfExists(temporary); }
        System.out.println("Patched one 32-bit launcher gate in " + output);
    }
}
