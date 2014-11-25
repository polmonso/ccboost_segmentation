# README #
### How do I get set up? ###

* Configuration

use the *task-based* branch

add -std=c++11 flag

* Dependencies

- deps directory on iiboost. Find it here: https://github.com/quimnuss/iiboost/tree/master/deps
- ITK (probably 4)
- VTK (probably 4)
- qt4
- openmp


*build

On the ccboost-segmentation directory
```
#!bash
mkdir build
cd build
ccmake ..
make

```
If you compile with -j the first time, it will fail because it has to download some dependencies
You'll get a Espina missing warning, it's fine if you want to use the standalone version only.


*Deployment instructions

the binary is ccbooststandalone and is ran like this:

*Running


```
#!bash

./ccbooststandalone -g groundtruth.tif -i volumeinput.tif -o predictionoutputfilename.tif
```


i.e.


```
#!bash

./ccbooststandalone -g Madrid_Train_espinagt_many.tif -i Madrid_Train.tif -o predict.tif
```