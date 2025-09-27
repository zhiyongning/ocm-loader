## OCM Loader
OCM Loader is a OCMAM based C++ application which can be used by OCM Viewer to download OCM data from HERE platform.

## Key features:
1. Download OCM data according to specific OCM layer group 
2. Download OCM data according to a coordinate point or bounding box
3. Download OCM data according to a filter

## Build Procedures
1. In ocm-loader directory, run the following commands
      ```bash
        mkdir build
        cd build
        cmake ..
        make -j4
        ```
 2. Then 2 excecutable files are generated in build/bin folder
    ```
    ocm-loader
    ├── build
    │   ├── bin
    │       ├── mytest
    │       └── ocm-loader
    ├── README.md
    ├ .....   
    ```                   

## Examples to execute the ocm-loader
1. ocm-loader isa point:13.08836,52.33812
2. ocm-loader rendering bbox:13.08836,52.33812,13.761,52.6755
3. ocm-loader isa point:13.08836,52.33812 filter:AND(forward_speed_limit=30)
