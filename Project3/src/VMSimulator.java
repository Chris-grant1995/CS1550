/**
 * Created by christophergrant on 3/20/16.
 */
import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.HashMap;
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
        HashMap<Integer,PTE> pageTable = new HashMap<>();
        Scanner scan = new Scanner(new File(tracefile));

        int hands = 0;
        System.out.println("Creating pageTable");
        for(int i =0; i< 1024*1024; i++){
            PTE temp = new PTE();
            pageTable.put(i,temp);
        }
        for(int i = 0; i<frames; i++){
            pageFrames[i] = -1;
        }
        int cur = 0; // current frame
        while (scan.hasNext()){
            String[] line = scan.nextLine().split(" ");
            int pageNum = Integer.decode("0x" + line[0].substring(0,5)); // 0,5 Because our page table doesn't support 8 hex digits worth of addresses
            PTE entry = pageTable.get(pageNum);
            entry.i = pageNum;
            entry.reference = true;
            if (line[1].equals("W")){
                entry.dirty = true;
            }
            if(!entry.v){
                //It is not valid, so thus page fault
                pageFaults++;
                if(cur <frames){
                    //No eviction
                    pageFrames[cur] = pageNum;
                    entry.frame = cur;
                    entry.v = true;
                    cur++;

                }
                else{
                    //Evicting
                    int pageToEvict =0;
                    boolean found = false;
                    while(!found){
                        if (hands==frames){
                            hands =0;
                        }
                        if(!pageTable.get(pageFrames[hands]).reference){
                            //If the reference bit is 0, evict
                            pageToEvict = pageFrames[hands];
                            found = true;
                        }
                        else{
                            pageTable.get(pageFrames[hands]).reference = false;
                        }
                        hands++;
                    }
                    PTE evict = pageTable.get(pageToEvict);
                    if(evict.dirty){
                        writes++;
                    }
                    pageFrames[evict.frame] = entry.i;
                    entry.frame = evict.frame;
                    entry.v = true;
                    evict.dirty = false;
                    evict.reference = false;
                    evict.v = false;
                    evict.frame = -1;
                    pageTable.put(pageToEvict,entry);
                }

            }
            else {
                //System.out.println("NoPageFault");
            }
            pageTable.put(pageNum,entry);
            memaccess++;
        }
        System.out.println("Done");
        System.out.println("Clock");
        System.out.println("Number of Frames: "+ frames);
        System.out.println("Total Memory Accesses: "+ memaccess);
        System.out.println("Total Page Faults "+ pageFaults);
        System.out.println("Total Writes to Disk "+ writes);
        //System.out.println("Done");
    }
    public void nru (int frames, String tracefile, int refresh)throws FileNotFoundException{
        int memaccess =0;
        int pageFaults = 0;
        int writes =0;
        int[] pageFrames = new int[frames];
        HashMap<Integer,PTE> pageTable = new HashMap<>();

        System.out.println("Creating Page Table");
        for (int i =0; i< 1024*1024; i++){
            PTE temp = new PTE();
            pageTable.put(i,temp);
        }
        for(int i =0; i<frames; i++){
            pageFrames[i] = -1;
        }
        int cur =0;
        Scanner scan = new Scanner(new File(tracefile));
        while(scan.hasNext()){
            if(memaccess % refresh == 0){
                for(int i =0; i< cur; i++){
                    PTE temp = pageTable.get(pageFrames[i]);
                    temp.reference = false;
                    pageTable.put(temp.i, temp);
                }
            }
            String [] line = scan.nextLine().split(" ");
            int pageNum = Integer.decode("0x" + line[0].substring(0,5));
            PTE entry = pageTable.get(pageNum);
            entry.i = pageNum;
            entry.reference = true;
            if (line[1].equals("W")){
                entry.dirty = true;
            }
            if(!entry.v){
                pageFaults++;
                if(cur < frames){
                    pageFrames[cur] = pageNum;
                    entry.frame = cur;
                    entry.v = true;
                    cur++;
                }
                else{
                    PTE pageToEvict = null;
                    boolean found = false;
                    while(!found){
                        for(int i =0; i<frames; i++){
                            PTE temp = pageTable.get(pageFrames[i]);
                            if(!temp.reference && !temp.dirty && temp.v){
                                entry.frame = temp.frame;
                                pageFrames[entry.frame] = entry.i;
                                temp.v = false;
                                temp.dirty = false;
                                temp.reference = false;
                                temp.frame = -1;
                                pageTable.put(temp.i, temp);
                                entry.v = true;
                                pageTable.put(entry.i, entry);
                                //found = true;
                                break;
                            }
                            else{
                                if(!temp.reference && temp.dirty && temp.v){
                                    pageToEvict.copy(temp);
                                    //continue;
                                }
                                else if(temp.reference && !temp.dirty && temp.v && pageToEvict == null){
                                    pageToEvict.copy(temp);
                                    //continue;
                                }
                                else if(temp.reference && temp.dirty && temp.v && pageToEvict == null){
                                    pageToEvict.copy(temp);
                                }
                            }

                        }
                        if(pageToEvict == null){
                            System.out.println("Uh oh");
                        }
                        entry.frame = pageToEvict.frame;
                        if(pageToEvict.dirty){
                            writes++;
                        }
                        pageFrames[entry.frame] = entry.i;
                        pageToEvict.v = false;
                        pageToEvict.dirty = false;
                        pageToEvict.frame = -1;
                        pageToEvict.reference = false;
                        pageTable.put(pageToEvict.i, pageToEvict);
                        entry.v = true;
                        pageTable.put(entry.i, entry);
                        found = true;
                    }
                }
            }
            pageTable.put(pageNum,entry);
            memaccess++;
        }

    }
    public void aging(int frames, String tracefile, int refresh){
        int memaccess =0;
        int pageFaults = 0;
        int writes =0;
        int[] pageFrames = new int[frames];
    }
}
