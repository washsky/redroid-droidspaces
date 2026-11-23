package lab.arm13.gpuprobe;
import android.media.MediaCodec;
import java.util.List;
public final class CodecKeys {
 public static void main(String[] args) throws Exception {
  MediaCodec c=MediaCodec.createByCodecName("c2.qti.avc.encoder");
  try {
   List<String> keys=c.getSupportedVendorParameters();
   System.out.println("VENDOR_KEY_COUNT="+keys.size());
   for(String k:keys)if(k.contains("linear")||k.contains("ubwc")||k.contains("color"))
    System.out.println("VENDOR_KEY="+k+" TYPE="+c.getParameterDescriptor(k).getType());
  } finally {c.release();}
 }
}
