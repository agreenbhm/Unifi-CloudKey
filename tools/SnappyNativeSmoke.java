import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import org.xerial.snappy.Snappy;

/** Exercise the application's actual Java binding against the configured native library. */
public final class SnappyNativeSmoke {
    public static void main(String[] arguments) throws Exception {
        String directory = System.getProperty("org.xerial.snappy.lib.path");
        if (directory == null || !Files.isRegularFile(Path.of(directory, System.mapLibraryName("snappyjava"))))
            throw new IllegalStateException("Configured native Snappy library is missing: " + directory);
        byte[][] samples = {
            new byte[0],
            "UniFi ARMhf Snappy native compression check".getBytes(StandardCharsets.UTF_8),
            new byte[8192]
        };
        for (int i = 0; i < samples[2].length; i++) samples[2][i] = (byte) (i * 31);
        for (byte[] original : samples) {
            byte[] restored = Snappy.uncompress(Snappy.compress(original));
            if (!Arrays.equals(original, restored)) throw new AssertionError("Native Snappy round trip failed");
        }
        System.out.println("Native Snappy round trips passed on " + System.getProperty("os.arch")
                + " using " + directory);
    }
}
