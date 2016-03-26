/**
 * Created by christophergrant on 3/26/16.
 */
public class Testing {
    public static void main(String[] args) {
        int t = 0;
        String s2 = String.format("%8s", Integer.toBinaryString(t & 0xFF)).replace(' ', '0');
        System.out.println(s2);

        t += 128;
        s2 = String.format("%8s", Integer.toBinaryString(t & 0xFF)).replace(' ', '0');
        System.out.println(s2);

        System.out.println("UnSigned");
        t = t >>> 1;
        s2 = String.format("%8s", Integer.toBinaryString(t & 0xFF)).replace(' ', '0');
        System.out.println(s2);
    }
}
