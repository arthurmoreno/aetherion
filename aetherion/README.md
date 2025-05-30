


Ideally for system package let's try to use the system package managers:
* sudo apt-get -> for ubuntu (debian based OS)
* vpkg -> windows

```
cd lifesimcore
conda activate life-sim-312
conda install -c conda-forge nanobind
conda install -c conda-forge numpy
# conda install conda-forge::openvdb -> openvdb is not working on conda
conda install -c conda-forge libstdcxx-ng
conda install -c conda-forge libstdcxx-ng-static
pip install -r dev-requirements.txt
```

??? -- 
mv $CONDA_PREFIX/lib/libstdc++.so.6 $CONDA_PREFIX/lib/libstdc++.so.6.bak


# Building:

1. if not on current folder:
    cd lifesimcore (current folder)
2. conda deactivate
3. conda activate life-sim-312
4. make build-and-install


# Issues:

OpenVDB it needs to be build with the system library not conta
