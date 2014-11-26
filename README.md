# README #
### How do I get set up? ###

* Configuration

use the *task-based* branch

add -std=c++11 flag

* Dependencies

- iiboost and its deps directory (you'll fetch it with git-submodule below)
- ITK (probably 4)
- VTK (probably 4)
- qt4
- openmp

* IIBoost submodule

If you haven't cloned the repository with modules, namely

```
#!bash
git clone --recursive git@bitbucket.org:polmonso/ccboost-segmentation.git
```

you can clone the submodules after the fact with

```
#!bash
cd ccboost-segmentation
git submodule update --init --recursive
```

* build

On the ccboost-segmentation directory
```
#!bash
mkdir build
cd build
cmake .. -DCMAKE_CXX_FLAGS="-std=c++11"
make

```
If you compile with -j the first time, it will fail because it has to download some dependencies
You'll get a Espina missing warning, it's fine if you want to use the standalone version only.

If you need to add the dependencies path use `ccmake ..` to add them


* Running

the binary is ccbooststandalone and is ran like this:

```
#!bash

./ccbooststandalone -g groundtruth.tif -i volumeinput.tif -o predictionoutputfilename.tif
```


i.e.


```
#!bash

./ccbooststandalone -g Madrid_Train_espinagt_many.tif -i Madrid_Train.tif -o predict.tif
```
