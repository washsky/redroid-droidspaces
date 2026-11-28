package lab.arm13.probe;

import android.app.Activity;
import android.os.Bundle;
import android.os.SharedMemory;
import android.util.Log;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.File;
import java.net.InetAddress;
import java.net.URL;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Arrays;
import javax.net.ssl.HttpsURLConnection;

/** Small source-backed release probe. No root, account, game or automation. */
public final class Probe extends Activity {
    private TextView output;
    private void report(String message) {
        Log.i("Arm13ReleaseProbe", message);
        runOnUiThread(() -> output.append(message + "\n"));
    }
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        output = new TextView(this);
        output.setPadding(24, 24, 24, 24);
        output.setTextIsSelectable(true);
        ScrollView scroll = new ScrollView(this);
        scroll.addView(output);
        setContentView(scroll);
        final String address = getIntent().getStringExtra("url") == null
                ? "https://example.com/" : getIntent().getStringExtra("url");
        new Thread(() -> {
            report("UID=" + android.os.Process.myUid());
            try {
                File file = new File(getFilesDir(), "release-probe.txt");
                byte[] value = "ARM13 private storage probe".getBytes(StandardCharsets.UTF_8);
                Files.write(file.toPath(), value);
                if (!Arrays.equals(value, Files.readAllBytes(file.toPath()))) throw new Exception("private storage mismatch");
                if (!file.delete()) throw new Exception("private storage cleanup failed");
                report("PRIVATE_STORAGE=PASS");
                try (SharedMemory memory = SharedMemory.create("arm13-release", 4096)) {
                    ByteBuffer buffer = memory.mapReadWrite();
                    try {
                        buffer.putInt(0, 0x523133);
                        if (buffer.getInt(0) != 0x523133) throw new Exception("shared memory mismatch");
                    } finally { SharedMemory.unmap(buffer); }
                }
                report("SHARED_MEMORY=PASS");
                URL url = new URL(address);
                if (!"https".equals(url.getProtocol())) throw new Exception("HTTPS URL required");
                report("DNS=" + Arrays.toString(InetAddress.getAllByName(url.getHost())));
                HttpsURLConnection connection = (HttpsURLConnection) url.openConnection();
                connection.setConnectTimeout(12000);
                connection.setReadTimeout(12000);
                try {
                    int code = connection.getResponseCode();
                    report("HTTPS_CODE=" + code + " CIPHER=" + connection.getCipherSuite());
                    if (code != 200 && code != 204) throw new Exception("unexpected HTTPS status " + code);
                } finally { connection.disconnect(); }
                report("API_AND_HTTPS=PASS");
            } catch (Exception error) { report("ERROR=" + error); }
        }, "arm13-release-probe").start();
    }
}
