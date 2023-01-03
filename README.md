# Gardener Fiber-Based Job System

Inspired by the Game Engine Architecture book and Naughty Dog's fiber-based job system slides on GDC, this project aims to serve as an implementation reference and a personal game engine building block.

## Example

```
class CustomTask
{
    static void MyTaskFunc(CustomTask* pThisTask){...}
    uint64_t m_taskId;
}
void main()
{
    ...
    Gardener::JobSystem jobSys(runThreadNum);
    CustomTask task;
    jobSys.AddAJob(&task->MyTaskFunc, &task, tasks.m_taskId);
    ...
    jobSys.WaitJobsComplete();
    ...
}
```

## Build

This project relies on Boost C++ and currently only works on WindowOS. Please install the [boost library](https://sourceforge.net/projects/boost/files/boost-binaries/) before using this repo. 

There are three examples project under the `example` folder. In the project folder, you can use the following building command to generate the project: 

`cmake -B build -G "Visual Studio 16 2019" -DBOOST_ROOT=C:\boost_1_81_0`

Please let me know if you need any help. Discord Server: https://discord.gg/ZmXaZUJzvF

## Reference

* Game Engine Architecture, Third Edition
* [Parallelizing the Naughty Dog engine using fibers](http://www.swedishcoding.com/2015/03/08/gdc-2015-presentation-parallelizing-the-naughty-dog-engine/)
