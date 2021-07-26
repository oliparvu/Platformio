Import("env", "projenv")
import shutil

print ("Post build scripts")

def after_build(source, target, env):
    print ("*****")

   

    hexname = env.get("PROGNAME")
    hexpath = ".pio\\build\\esp01_1m\\" + hexname + ".bin"
   
    
    destination_path = "u:\\html\\Mesh\\" + hexname + ".bin"
    try:
        shutil.copyfile(hexpath, destination_path)
    except:
        print("Cant find " +  hexpath)
    # do some actions

env.AddPostAction("buildprog", after_build)
