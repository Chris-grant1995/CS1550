/**
 * Created by christophergrant on 3/20/16.
 * Usage: vmsim –n <numframes> -a <opt|clock|nru|aging> [-r <refresh>] <tracefile>
 */
public class vmsim {
    public static void main(String[] args){

        int frameNo = 0;
        String algo = "";
        int refresh = 0;
        String trace = "";
        VMSimulator sim = new VMSimulator();
        for(int i =0; i<args.length; i++){
            if(args[i].equals("-n")){
                frameNo = Integer.parseInt(args[i+1]);
                if(frameNo!=8 && frameNo!=16 && frameNo!=32 && frameNo!=64 ){
                    System.out.println("Invalid frame size, use 8,16,32,64 instead");
                    return;
                }
            }
            else if(args[i].equals("-a")){
                algo = args[i+1];
            }
            else if(args[i].equals("-r")){
                refresh = Integer.parseInt(args[i+1]);
            }
        }
        trace = args[args.length -1];

        if(algo.equals("opt")){
            sim.opt(frameNo,trace);
        }
        else if(algo.equals("clock")){
            sim.clock(frameNo, trace);
        }
        else if (algo.equals("nru")){
            sim.nru(frameNo,trace,refresh);
        }
        else if(algo.equals("aging")){
            sim.aging(frameNo,trace,refresh);
        }
        else{
            System.out.println("Error");
        }
    }
}
