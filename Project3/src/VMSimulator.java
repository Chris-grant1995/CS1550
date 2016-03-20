/**
 * Created by christophergrant on 3/20/16.
 */
import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.Scanner;
public class VMSimulator {
    public VMSimulator(){

    }

    public void opt(int frames, String tracefile){
        int memaccess =0;
        int pageFaults = 0;
        int writes =0;
        int[] pageFrames = new int[frames];


    }
    public void clock(int frames, String tracefile)throws FileNotFoundException{
        int memaccess =0;
        int pageFaults = 0;
        int writes =0;
        int[] pageFrames = new int[frames];
        ArrayList<PTE> pageTable = new ArrayList<>();
        Scanner scan = new Scanner(new File(tracefile));

        int hands = 0;
        System.out.println("Creating pageTable");
        for(int i =0; i< 1024*1024; i++){
            PTE temp = new PTE();
            pageTable.add(i,temp);
        }
        for(int i = 0; i<frames; i++){
            pageFrames[i] = -1;
        }
        int cur = 0; // current frame
        while (scan.hasNext()){
            String[] line = scan.nextLine().split(" ");
            int pageNum = Integer.decode("0x" + line[0]);
            PTE entry = pageTable.get(pageNum);
        }
        System.out.println("Done");
    }
    public void nru (int frames, String tracefile, int refresh){
        int memaccess =0;
        int pageFaults = 0;
        int writes =0;
        int[] pageFrames = new int[frames];
    }
    public void aging(int frames, String tracefile, int refresh){
        int memaccess =0;
        int pageFaults = 0;
        int writes =0;
        int[] pageFrames = new int[frames];
    }
}
