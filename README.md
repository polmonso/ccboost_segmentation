# README #

EspINA Neuron Analyzer ccboost synapse/mitochondria segmentation plugin and import plugin.

The current version of the plugin is under development and uses the develop branch of EspINA, about to be released. 
Currently, commit ```8244e5be``` which corresponds to espina 2.0.3 is supported.

![EspINA Screenshot](https://documents.epfl.ch/groups/c/cv/cvlab-unit/public/espina/espina1_screenshot.png "Mitochondria segmentation")

### How do I get set up? ###

* Configuration

add ```-std=c++11``` flag

* Dependencies

- iiboost and its deps directory (you'll fetch it with git-submodule below)
- ITK (probably 4)
- VTK (probably 6)
- qt4
- openmp
and
- Espina (see below)

Currently, version ```2.0.3``` is the latest supported version (commit ```8244e5be```)
Checkout with git checkout ```8244e5be```.

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

Remember to switch to the appropiated ```ccboost-segmentation``` branch.

* build

On the ccboost-segmentation directory

```
#!bash
mkdir build
cd build
cmake .. -DCMAKE_CXX_FLAGS="-std=c++11"
make

```
You'll get a Espina missing warning, it's fine if you want to use the standalone version only.

If you need to add the dependencies path use `ccmake ..` to add them

* Running

the binary is ccbooststandalone and is ran like this:

```
#!bash

./ccbooststandalone -t trainingvolume.tif -g groundtruth.tif -i volumeinput.tif -o predictionoutputfilename.tif
```


i.e.


```
#!bash

./ccbooststandalone -g Madrid_Train_espinagt_many.tif -i Madrid_Train.tif -o predict.tif
```

## Working with git submodules
tl; dr

After Git 1.8.2, update the submodule spear to master branch with:
```
git submodule update --remote
```

--

Before Git 1.8.2

* pulling the latest submodule code

If submodule upstream is updated and you now want to update, change to the submodule directory

```
cd submodule_dir
```
checkout desired branch and update
```
git checkout master
git pull
```

Then get back to your project root. Now the submodules are in the state you want, so you can commit the changes

```
cd ..
git commit -am "Pulled down update to submodule_dir"
```

* [Git 1.8.2][1] added the possibility to track branches. 

> "`git submodule`" started learning a new mode to **integrate with the tip of the remote branch** (as opposed to integrating with the commit recorded in the superproject's gitlink).
    
    # add submodule to track master branch
    git submodule add -b master [URL to Git repo];

    # update your submodule
    git submodule update --remote 

See also the [Vogella's tutorial on submodules][2].

See "**[How to make an existing submodule track a branch][3]**" (if you had a submodule already present you wish now would track a branch)

[1]: https://github.com/git/git/blob/master/Documentation/RelNotes/1.8.2.txt
[2]: http://www.vogella.com/articles/Git/article.html#submodules
[3]: http://stackoverflow.com/a/18799234/6309
 

## Espina installation troubleshooting

- Use the official [Espina](http://cajalbbp.cesvima.upm.es/espina/) or request access to the private repository to [Jorge Peña](https://bitbucket.org/jorgepenapastor) 

The instruction to install it are [here](https://bitbucket.org/espina-developers/espina). Follow those instructions.
When installing ```VTK```, you might have to turn ON the module ```Module_vtkInfovisBoostGraphAlg```, additionally to ```Module_vtkInfovisBoost```.

Right now, only espina ```2.0.3``` is supported. Checkout SHA1 ```8244e5be4283fbce6f6f5ba6bc1dec1bd9b7f4c2``` on branch ```develop```

- You can get some of the dependencies ([xlslib](http://sourceforge.net/projects/xlslib/files/) and [quazip](http://sourceforge.net/projects/quazip/))

It has been tested with versions ```2.4.0``` and ```0.6.2``` respectively. 

- If you get a message saying 'ill-formed pair', there's a problem with boost. Check that you have boost 1.55.

If you don't have the appropriate boost libraries, you can download them and put the boost's library paths directly on ccmake. 

- If quazip is expecting ```quazip/``` includes, change the ```include/quazip``` to ```include/``` on the cmake variable

If you don't have root privileges to install quazip, you can install it anywhere 
with ```make DESTDIR=.``` and then move the include and lib directories wherever you like .

This is the ```espina/CMakeLists.txt``` modified beginning:

```
set(XLSLIB_INCLUDE_DIR "/home/monso/code/espina-project2/xlslib/src/" CACHE PATH "xlslib's src directory")
set(XLSLIB_LIBRARY "/home/monso/code/espina-project2/xlslib/src/.libs/libxls.so" CACHE FILE "xlslib lib, usually at xlslib/.libs/libxls.so")

link_directories("/usr/lib/x86_64-linux-gnu/")

add_definitions(
  -std=c++11
  -fpermissive
  )
  
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMake/Modules/")

find_package(QuaZip REQUIRED)
include_directories(${QUAZIP_INCLUDE_DIRS})
#if (NOT DEFINED MINGW AND NOT DEFINED MSVC)
#  include(${QUAZIP_USE_FILE})
#endif (NOT DEFINED MINGW AND NOT DEFINED MSVC)

find_package(Boost REQUIRED COMPONENTS  graph regex)
link_directories ( ${Boost_LIBRARY_DIRS} )
include_directories ( ${Boost_INCLUDE_DIRS} )
```


# Detailed Description of the Code Structure

![A workflow diagram](https://documents.epfl.ch/groups/c/cv/cvlab-unit/public/espina/flow.png "Basic program workflow")
![The ITK pipeline](https://documents.epfl.ch/groups/c/cv/cvlab-unit/public/espina/itkpipeline.png "Underlying ITK pipeline")
## CvlabPanel

The code provides a Cvlab Qt Panel for espina which connects with Ccboost Segmentation Plugin, who manages everything.

The plugin provides three tasks:

* Import segmentation ![Icon](https://rawgit.com/quimnuss/ccboost_segmentation/master/GUI/rsc/LoadLabels.svg)

This triggers the Import task which reads an image and imports its labels into Espina Segmentations.

* Synapse Segmentation ![Icon](https://rawgit.com/quimnuss/ccboost_segmentation/master/GUI/rsc/SynapseDetection.svg)

This button triggers a Ccboost segmentation task with the element selected to Synapse, which means that will search the Espina's taxonomy tree for Synapse examples (either by Category Synapse or tag yessynapse) and negative examples by Background category or nosynapse tag.

To add tags within espina look at the segmentation list panel.

* Mitochondria Segmentation ![Icon](https://rawgit.com/quimnuss/ccboost_segmentation/master/GUI/rsc/MitochondriaDetection.svg)


Same as the Synapse Segmentation but now it searches for the Mitochondria segmentation category or yesmitochondria|nomitochondria tags.

All three end up creating a preview that has to be thresholded using this same panel. Once happy, press the Extract Segmentation button [image].


These tasks are created at the Ccboost Plugin, which is the manager of our algorithms. To be consequently executed as a Espina Task, a child of QThread. Once the task finishes it is captured by the plugin again. 

This can be seen at ```CcboostSegmentationPlugin::create*Task( ... ):292```

     SchedulerSPtr scheduler = getScheduler();
     CCB::ImportTaskSPtr importTask{new CCB::ImportTask(channel, scheduler, filename)};
     struct CcboostSegmentationPlugin::ImportData data;
     m_executingImportTasks.insert(importTask.get(), data);
     connect(importTask.get(), SIGNAL(finished()), this, SLOT(finishedImportTask()));
     Task::submit(importTask);

The other particulars of the Ccboost Plugin follow. The description of the Ccboost and Import tasks are next.


## Ccboost Segmentation Plugin

The Ccboost Segmentation Plugin creates the tasks and handles its signals. It provides an interface of communication between the tasks and the main app.

It also handles the finishing of the tasks, which for the Import task involves creating the Espina segmentations and for the ```CcboostTask```, notifying the ```CvPanel```'s ```Preview```.

## Ccboost Task

The ccboost task prepares all the data and configuration to be fetched to the ccboost. It does the memory checks and the pre and postprocessing.

When all the data is ready, it calls the ```CcboostAdapter::core```, the ```iiboost``` wrapper which formats the data as expected by iiboost.

## CcboostAdapter

Takes the prepared data and computes and adds its associated features if necessary.

## iiboost

For iiboost information, see its [github page](https://github.com/cbecker/iiboost).

## Import Task

The import task takes a float image and obtains the labelmap which is then converted to espina segmentations by the ```CcboostSegmentationPlugin```
