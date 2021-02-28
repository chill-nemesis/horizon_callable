***********************
The **callable** module
***********************

.. See https://raw.githubusercontent.com/readthedocs/sphinx_rtd_theme/master/README.rst for an example of an ReadMe with rst syntax
.. Cheat-Sheet for RST language: thomas-cokelaer.info/tutorials/sphinx/rest_syntax.html
.. TODO: add build/cover info
.. TODO: auto-generate some/most? of the readme?

This module is providing advanced control over task and task-result encapsulation. It provides a layer of abstraction for advanced function calls
(e.g. Functions as parameters or callbacks).
In addition, the **callable** module is designed with parallelism in mind - in fact, this module is the basis of the `parallel module`_ - and
provides std::future-like result-handling behaviour.

.. _parallel module: https://git.ch3ll.com/horizon-engine/parallel

Installation
============
Dependencies
------------
.. TODO: auto-generate dependencies

* CMake_, 3.15 or greater
* `preprocessor module`_
* cmake-helpers from the superbuild_

Currently, the modules do not have a dependency-resolving mechanism (e.g. the required other modules are not included via ``git submodule``) so you
need to download and configure them manually. In addition, you need to add the ``cmake-folder`` from the superbuild_ to the CMake-module path to
compile the module.

.. _CMake: https://cmake.org/download/
.. _preprocessor module: https://git.ch3ll.com/horizon-engine/preprocessor
.. _superbuild: https://git.ch3ll.com/horizon-engine/superbuild


Configuration
-------------
.. TODO: this should prlly be imported from each module (maybe like .. include:: docs/Configuration.rst)
.. TODO: build docs is an option...

Currently, there are no configuration options for **callable**

Building
--------
.. TODO: c++-version from cmake

**callable** is a C++-17 library. Make sure your compiler is up to date and supports the language features.

If you are not compiling **callable** as part of the superbuild_, make sure to add the ``cmake-folder`` to the `CMake-Module path`_:
If you are just compiling this library, you can set the ``CMAKE_MODULE_PATH`` variable to the helper-folder.
If this library is part of another build, it is recommended to append the helper-folder to the ``CMAKE_MODULE_PATH``-variable (in the topmost
CMakeLists.txt, before adding the **callable** module):

.. code-block:: CMake

    set(CMAKE_MODULE_PATH "<PATH>/<TO>/<HELPER_FOLDER>" ${CMAKE_MODULE_PATH})


.. note:: Currently, all dependency projects will not be built by the module, so you need to add the respective header and libraries to cmake.


.. _CMake-Module path: https://cmake.org/cmake/help/latest/variable/CMAKE_MODULE_PATH.html