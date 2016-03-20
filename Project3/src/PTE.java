/**
 * Created by christophergrant on 3/20/16.
 */
public class PTE {
    int frame;
    int i; //index
    boolean v; // valid
    boolean dirty;
    boolean reference;

    public PTE(){
        frame = -1;
        i =0;
        v = false;
        dirty = false;
        reference = false;
    }
    public void copy(PTE entry){
        frame = entry.frame;
        i = entry.i;
        v = entry.v;
        dirty = entry.dirty;
        reference = entry.reference;
    }
}
