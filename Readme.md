![OpenMCx](open_mcx_logo.png "OpenMCx")
=======================================

![OpenMCx build](https://github.com/eclipse/openmcx/actions/workflows/openmcx-build.yml/badge.svg)

OpenMCx is an extendable, tool-neutral co-simulation framework based on [Modelica](https://www.modelica.org/)
standards with the goal of supporting advanced simulation applications with a heterogenous toolchain
in a distributed collaborative development process. It provides a flexible way of combining simulation
models from different vendors and sources into one co-simulation model which can also be executed in
a scalable computing environment.

With its component-based architecture, OpenMCx provides an easy way to integrate simulation models
from different vendors via:
* standardized interfaces (e.g. [FMI - Functional Mock-up Interface](https://fmi-standard.org/) or
                                [DCP - Distributed Co-Simulation Protocol](https://dcp-standard.org/))
* built-in interfaces through which the framework already supports specific tools
* custom interfaces, which can be easily implemented by the user and plugged into the existing framework

The definition and parameterization of OpenMCx models is based on the
[SSP - System Structure and Parameterization](https://ssp-standard.org/) standard which can also be easily extended
thanks to its annotation concept.

OpenMCx was designed as generic as possible to support a wide range of application areas. As it was made
publicly available as part of the [OpenADx](https://openadx.eclipse.org/) initiative, one of its most
interesting application areas is the virtual validation of ADAS/AD functions, but it is not restricted to
that area alone.

OpenMCx contains parts of [AVL](https://www.avl.com/)'s open model integration and co-simulation platform - [Model.CONNECT&trade;](https://www.avl.com/-/model-connect-).
Both of them are being continuously developed and enhanced.

For a more comprehensive list of supported interfaces and planned extensions in OpenMCx take a look at
[Roadmap](#roadmap).

# Getting Started

For a quick-start guide with Visual Studio Code, see [Getting Started in Visual Studio Code](Readme_VS_Code.md).

To manually build `OpenMCx`, follow the instructions below:

## Prerequisites
In order to be able to build OpenMCx, the following tools must be installed:

__Linux__:
- gcc >= 8
- CMake >= 3.14
- Make
- git
- Python (2 or 3)

__Windows__:
- Microsoft Visual Studio 2015 or later
- CMake >= 3.14
- git
- Python (2 or 3)

## Dependencies
The build of OpenMCx needs the following libraries to be installed on the build machine:
- libxml2
- libzip
- zlib

E.g., on Ubuntu, run
```sh
sudo apt install -y libxml2-dev zlib1g-dev libzip-dev
```

On Windows, `build.bat` automatically takes care of installing these dependencies using `vcpkg`.

### Docker

Alternatively, the [`Dockerfile`](docker/Dockerfile) can be used to build an image that contains all prerequisites:
```sh
cd docker
docker build .
```

Similarly, a `devcontainer.json` is provided to automatically create a development container in Visual Studio Code (see [Getting Started in Visual Studio Code](Readme_VS_Code.md) for details).

## Building
OpenMCx uses `cmake` as its build system and uses python to process SSP schema files during the
configuration. Convenience scripts are available to make the build process easier:

__Linux__:
```sh
./build.sh
```

__Windows__:
```bat
.\build.bat
```

This will generate and install the OpenMCx executable (named `openmcx` in the `install` folder).

To get an `openmcx` executable with DCP support (currently only supported for windows), run:
```bat
.\build_with_dcp.bat
```

__Note__: `cmake`, `make`, `git` and `python` MUST be present on PATH when running the scripts
listed above.

## Running

OpenMCx has one mandatory argument - the model definition file. The model is defined via SSP
(see [Model Definition](#model-definition)). In particular, OpenMCx expects an unpacked SSP
project and should be called with the corresponding `.ssd` file as its command line argument:

__Linux__

```sh
./install/openmcx -v ./examples/getting_started/model.ssd
```

__Windows__

```bat
.\install\openmcx.exe -v .\examples\getting_started\model.ssd
```

The `-v` is used to display debug log messages. Once the simulation is done, results will be
available in the `results` folder of the current working directory.


The behavior of the `openmcx` executable can be tweaked via the following optional arguments:
- `--tempdir`, `-t` - temporary directory used to store intermediate files (default: `temp`)
- `--resultdir`, `-r` - directory where results will be stored (default: `results`)
- `--log`, `-L` - log file (default: `simulation.log`)
- `--verbose`, `-v` - enables debug logging (if enabled an additional log file named `mcx_all.log`
                      will be created)
- `--enablegraphs`, `-g` - generate a graphical representation of model dependencies
                           (produces a `.dot`-file which can be processed via e.g.
                           [Graphviz](https://graphviz.org/) or [WebGraphviz](http://www.webgraphviz.com/))

To run examples with DCP, the DCP slaves must be started first.

An example DCP slave gets built in the `\install\dcp\cosine` folder and can be started with:
```bat
.\install\dcp\cosine>slave-cosine.exe
```
Afterwards, the DCP example can be simulated:
```bat
.\install\openmcx.exe -v .\examples\dcp_fmu\model.ssd
```


# Model Definition
OpenMCx models are defined via the [SSP](https://ssp-standard.org/) standard. Supported is
a subset of the SSP standard as well as some OpenMCx-specific extensions. The extension mechanism
provided by SSP is used in order to introduce necessary concepts that don't exist in the original
SSP standard.

OpenMCx uses _system structure description_ files (`.ssd`) as inputs describing the model structure
and simulation parameters. SSP archives (`.ssp`) are not yet supported. Some other limitations
can be found [here](Limitations.md).

All parts of the graphical model representation (geometry, graphical elements etc.) are ignored as
they are not relevant for the simulation.


## Extensions
OpenMCx extends the SSP standard by defining additional MIME types and annotations.

In case the input SSD file does not use any of the OpenMCx specific extensions, OpenMCx will simulate
the described model assuming reasonable defaults for simulation parameters that are out of SSP scope.

### Example

A few examples demonstrating OpenMCx specific SSP extensions can be found in [examples](examples) with a short overview in [examples/Readme](examples/Readme.md). Most of the
examples contain descriptive comments describing extensions of interest.

A simple model containing a _Constant_ connected to an _FMU_
([Getting Started](examples/getting_started/model.ssd)) would look like:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<SystemStructureDescription xmlns="http://ssp-standard.org/SSP1/SystemStructureDescription"
                            xmlns:ssc="http://ssp-standard.org/SSP1/SystemStructureCommon"
                            name="Getting Started"
                            version="1.0">
    <System name="Root">
        <Elements>
            <!-- definition of OpenMCx built-in component -->
            <!-- source is empty as the signal generator doesn't have a source file/model -->
            <!-- a custom MIME type is used -->
            <Component name="Constant" source="" type="application/avl-mcx-constant">
                <Connectors>
                    <Connector name="out" kind="output">
                        <ssc:Real/>
                        <Annotations>
                            <!-- custom annotation for defining OpenMCx specific port data -->
                            <ssc:Annotation type="com.avl.model.connect.ssp.port"
                                            xmlns:mc="com.avl.model.connect.ssp.port">
                                <mc:Port>
                                    <!-- disable storing values of the port into the result files -->
                                    <mc:Real writeResults="false"/>
                                </mc:Port>
                            </ssc:Annotation>
                        </Annotations>
                    </Connector>
                </Connectors>

                <Annotations>
                    <!-- custom annotation for defining OpenMCx specific component data -->
                    <!-- each component might have custom specific data depending on the implementation -->
                    <ssc:Annotation type="com.avl.model.connect.ssp.component.constant"
                                    xmlns:mc="com.avl.model.connect.ssp.component.constant">
                        <mc:SpecificData>
                            <!-- the first connector of the constant will produce the value 3.0 -->
                            <mc:Real value="3.0"/>
                        </mc:SpecificData>
                    </ssc:Annotation>
                </Annotations>
            </Component>

            <Component name="FMU" source="../../external_data/gain.fmu" type="application/x-fmu-sharedlibrary">
                <Connectors>
                    <Connector name="out" kind="output">
                        <ssc:Real/>
                        <Annotations>
                            <ssc:Annotation type="com.avl.model.connect.ssp.port"
                                            xmlns:mc="com.avl.model.connect.ssp.port">
                                <!-- nameInModel is the name of connector in the underlying source -->
                                <!-- while interacting with the underlying source, OpenMCx will -->
                                <!-- use the nameInModel -->
                                <mc:Port nameInModel="real_out">
                                    <mc:Real/>
                                </mc:Port>
                            </ssc:Annotation>
                        </Annotations>
                    </Connector>
                    <Connector name="in" kind="input">
                        <ssc:Real/>
                        <Annotations>
                            <ssc:Annotation type="com.avl.model.connect.ssp.port"
                                            xmlns:mc="com.avl.model.connect.ssp.port">
                                <mc:Port nameInModel="real_in">
                                    <!-- default value used if nothing if the input is not connected -->
                                    <mc:Real default="8.0"/>
                                </mc:Port>
                            </ssc:Annotation>
                        </Annotations>
                    </Connector>
                </Connectors>
            </Component>
        </Elements>

        <Connections>
            <Connection startElement="Constant" startConnector="out" endElement="FMU" endConnector="in" />
        </Connections>
    </System>

    <DefaultExperiment startTime="0.0" stopTime="10.0">
        <Annotations>
            <!-- additional OpenMCx specific simulation parameters -->
            <ssc:Annotation type="com.avl.model.connect.ssp.task"
                            xmlns:mc="com.avl.model.connect.ssp.task">
                <!-- sequential execution of component steps -->
                <!-- synchronization time step size of 0.001 -->
                <!-- simulation will end when the stopTime is reached -->
                <mc:Task stepType="sequential" deltaTime="0.001" endType="end_time"/>
            </ssc:Annotation>
        </Annotations>
    </DefaultExperiment>
</SystemStructureDescription>
```

### MIME types
OpenMCx defines additional MIME types, besides the standard `application/x-fmu-sharedlibrary` one:

- `application/avl-mcx-integrator`
- `application/avl-mcx-vector-integrator`
- `application/avl-mcx-constant`

### Annotations
In order to extend SSP to support the definition of additional simulation and component parameters,
the annotations mechanism provided by SSP is used.

OpenMCx specific annotations can be defined for the following SSP elements:
- `Component`
    1. `com.avl.model.connect.ssp.component`
    2. `com.avl.model.connect.ssp.component.results`
    3. MIME type specific:
        * `com.avl.model.connect.ssp.component.fmu`

        * `com.avl.model.connect.ssp.component.integrator`
        * `com.avl.model.connect.ssp.component.vector_integrator`
        * `com.avl.model.connect.ssp.component.constant`

- `System` (not root)
    1. `com.avl.model.connect.ssp.component`

- `Connector`
    1. `com.avl.model.connect.ssp.port`
    2. `com.avl.model.connect.ssp.parameter`
    3. MIME type specific:
        * `com.avl.model.connect.ssp.parameter.fmu`

- `Connection`
    1. `com.avl.model.connect.ssp.connection`
    2. `com.avl.model.connect.ssp.connection.decoupling`
    3. `com.avl.model.connect.ssp.connection.inter_extrapolation`

- `DefaultExperiment`
    1. `com.avl.model.connect.ssp.task`
    2. `com.avl.model.connect.ssp.results`
    3. `com.avl.model.connect.ssp.config`

More information about these annotations can be deduced from the XML schema files in `scripts/SSP`.

# Unit Definitions
OpenMCx supports internal unit conversions. The list of units is
automatically taken from the `Units` element in the input `.ssd` file.

# Extending OpenMCx
Thanks to its modular design, it is easy to extend different parts of OpenMCx. Custom result
formats, step types, components or any other part of it can be implemented according
to ones needs.

## Defining Custom Components
The most common extension is the definition of components in order to support new
tools/interfaces. The definition of a new component includes the following steps:

1. A new `ComponentType` needs to be defined in the following files:
   - [ComponentTypes.c](src/components/ComponentTypes.c)
   - [ComponentTypes.h](src/components/ComponentTypes.h)
2. A new component needs to be derived from the `Component` class. The implementation of the
   new component class differs from component to component, but the interface that needs to
   be implemented is the same and defined in [Component.h](src/core/Component.h). An example
   component implementation can be found in:
   - [comp_integrator.c](src/components/comp_integrator.c)
   - [comp_integrator.h](src/components/comp_integrator.h)
3. Intermediate input representation classes need to be implemented for the new component.
   They store the data available in the input file and make it available to the rest of
   the framework. An example intermediate representation implementation can be found in:
   - [IntegratorInput.c](src/reader/model/components/specific_data/IntegratorInput.c)
   - [IntegratorInput.h](src/reader/model/components/specific_data/IntegratorInput.h)
4. The reading of the intermediate representation based on the given input file needs to
   be implemented by extending the following files:
   - [Components.c](src/reader/ssp/Components.c)
   - [SpecificData.c](src/reader/ssp/SpecificData.c)
   - [SpecificData.h](src/reader/ssp/SpecificData.h)
5. The newly defined component needs to be registered in the `ComponentFactory`:
   - [ComponentFactory.c](src/components/ComponentFactory.c)
6. In case the component implementation introduced new directories with source files, it
   might also be necessary to accordingly modify:
   - [CMakeLists.txt](src/CMakeLists.txt)

# RoadMap

- Co-simulation standards:
  - [x] [FMI - Functional Mock-up Interface](https://fmi-standard.org/)
  - [x] [SSP - System Structure and Parameterization](https://ssp-standard.org/)
  - [x] [DCP - Distributed Co-Simulation Protocol](https://dcp-standard.org/)
  - [ ] [ASAM OSI - Open Simulation Interface](https://www.asam.net/standards/detail/osi)


- Components and interfaces::
  - [ ] [FMI (1.0, 2.0)](https://fmi-standard.org/)
    - [x] [Co-Simulation FMU](https://fmi-standard.org/)
    - [ ] [Model Exchange FMU](https://fmi-standard.org/)
  - [ ] [FMI 3.0](https://fmi-standard.org/)
  - [x] [DCP](https://dcp-standard.org/)
  - [ ] Signal Sources:
    - [x] Constant
    - [ ] Signal file (csv)
  - [x] Signal integrator
  - [ ] [Python](https://www.python.org/)
  - [ ] [ROS2](https://github.com/ros2)
  - [ ] [CARLA](https://carla.org/)
  - [ ] [SUMO](https://www.eclipse.org/sumo/)
  - [ ] [Cloe](https://www.eclipse.org/cloe/)



# Contributing

See [CONTRIBUTING](./CONTRIBUTING.md).

# License

OpenMCx is licensed under the [Apache License Version 2.0](http://www.apache.org/licenses/LICENSE-2.0.txt).


# Contributors

See [CONTRIBUTORS](./CONTRIBUTORS.md).

-----

<img src="https://upload.wikimedia.org/wikipedia/commons/c/cc/AVL_Logo.jpg" alt="AVL" width="10%" align="right"/>
<img src="https://openadx.eclipse.org/images/openadx-logo.svg" alt="OpenADx" width="10%" alignt="right"/>
