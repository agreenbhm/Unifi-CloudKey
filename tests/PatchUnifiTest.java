import java.util.Arrays;
import java.nio.file.*;
import java.util.zip.*;
import jdk.internal.org.objectweb.asm.*;
import jdk.internal.org.objectweb.asm.tree.*;

public final class PatchUnifiTest {
    private static byte[] fixture(int gates, String message, int branch) {
        ClassWriter writer = new ClassWriter(0);
        writer.visit(Opcodes.V17, Opcodes.ACC_PUBLIC, "com/ubnt/ace/Launcher", null, "java/lang/Object", null);
        MethodVisitor method = writer.visitMethod(Opcodes.ACC_PUBLIC | Opcodes.ACC_STATIC,
                "main", "([Ljava/lang/String;)V", null, null);
        method.visitCode();
        for (int i = 0; i < gates; i++) {
            Label supported = new Label();
            method.visitMethodInsn(Opcodes.INVOKESTATIC, "com/ubnt/service/system/Detector", "is64Bit", "()Z", false);
            method.visitJumpInsn(branch, supported);
            method.visitLdcInsn(message);
            method.visitInsn(Opcodes.POP);
            method.visitInsn(Opcodes.ICONST_M1);
            method.visitMethodInsn(Opcodes.INVOKESTATIC, "java/lang/System", "exit", "(I)V", false);
            method.visitLabel(supported);
            method.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
        }
        method.visitInsn(Opcodes.RETURN);
        method.visitMaxs(1, 1);
        method.visitEnd();
        writer.visitEnd();
        return writer.toByteArray();
    }

    private static void rejects(byte[] input) {
        try { PatchUnifi.patch(input); }
        catch (IllegalArgumentException expected) { return; }
        throw new AssertionError("Unrecognized or ambiguous gate accepted");
    }

    public static void main(String[] arguments) throws Exception {
        byte[] input = fixture(1, "32-bit OS not supported!", Opcodes.IFNE);
        if (arguments.length == 1) {
            try (ZipOutputStream jar = new ZipOutputStream(Files.newOutputStream(Path.of(arguments[0])))) {
                jar.putNextEntry(new ZipEntry("META-INF/MANIFEST.MF"));
                jar.write("Manifest-Version: 1.0\r\nClass-Path: ./local/dependency.jar\r\n\r\n".getBytes(java.nio.charset.StandardCharsets.UTF_8));
                jar.closeEntry();
                jar.putNextEntry(new ZipEntry("com/ubnt/ace/Launcher.class"));
                jar.write(input);
                jar.closeEntry();
                jar.putNextEntry(new ZipEntry("untouched.txt"));
                jar.write(new byte[] {1, 2, 3});
                jar.closeEntry();
            }
        }
        byte[] patched = PatchUnifi.patch(input);
        if (Arrays.equals(input, patched)) throw new AssertionError("Class unchanged");
        ClassNode node = new ClassNode();
        new ClassReader(patched).accept(node, 0);
        MethodNode main = node.methods.get(0);
        if (main.instructions.getFirst().getOpcode() != Opcodes.ICONST_1)
            throw new AssertionError("Expected constant true at gate");
        // Verify stack maps/branch offsets by loading and executing the generated class.
        Class<?> loaded = new ClassLoader() {
            Class<?> define() { return defineClass(null, patched, 0, patched.length); }
        }.define();
        try { loaded.getMethod("main", String[].class).invoke(null, (Object) new String[0]); }
        catch (ReflectiveOperationException failure) { throw new AssertionError(failure); }
        rejects(fixture(0, "32-bit OS not supported!", Opcodes.IFNE));
        rejects(fixture(2, "32-bit OS not supported!", Opcodes.IFNE));
        rejects(fixture(1, "Some other failure", Opcodes.IFNE));
        rejects(fixture(1, "32-bit OS not supported!", Opcodes.IFEQ));
        rejects(patched);
        System.out.println("PASS: recognized gate executes; missing, ambiguous, changed, reversed, and already patched gates rejected");
    }
}
