import subprocess
import os
if __name__ == '__main__':
    iter = 1000000
    for threads in range(1, 21):
        dir = "results/testing/t%d_iter%d" % (threads, iter)
        if not os.path.exists(dir):
            os.makedirs(dir)
        print("Starting interface monitor")
        cmd = "sudo bwm-ng -I enp66s0f0 -o csv -u bytes -T rate -C ',' > %s/bandwidth.csv &" % (dir)
        os.system(cmd)
        print("Launching microbenchmark %d Threads %d Iterations " % (threads, iter))
        cmd = "sudo ./applications/bin/testing -c ./distmem_client.cnf -t %d -i %d" % (threads, iter)
        print("Executing %s" % cmd)
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
        p_status = p.wait()
        print("Stopping interface monitor")
        os.system("sudo killall bwm-ng")
