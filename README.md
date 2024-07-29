# GpgFrontend Modules

This repository hosts all the inherited module code for GpgFrontend. These
modules are packaged with the GpgFrontend main program by default. The module
code hosted in this repository needs to be reviewed by the project maintainer,
Saturneric, to ensure high security. Therefore, GpgFrontend automatically loads
these modules at startup by default. Users can also disable any or all of these
modules in the settings provided by GpgFrontend. On some operating system
platforms, GpgFrontend is released via compressed files rather than images, and
users can delete a specific module or all modules by removing the corresponding
dynamic link library (DLL).

## About the Modules

Module support is a key extension feature of GpgFrontend. By encapsulating
functionality into modules, users can tailor GpgFrontend to their needs. They
can temporarily disable or even delete a particular feature. This approach
avoids loading a large amount of code that not everyone may use into the
GpgFrontend main program. Consequently, the GpgFrontend main program becomes
more stable, concise, and focused on performing tasks well.

Another benefit of introducing module support is that anyone can participate
without needing to learn the architecture and various mechanisms of the
GpgFrontend main program. Through a brief API provided by the SDK, utilizing
easy-to-understand and flexible event and anchor mechanisms, modules can play
their roles. Users can write and publish their own modules or contribute to this
repository.

### SDK and API Usage

The SDK API uses C-style function parameters and ABI. Once the API stabilizes,
it will largely remain unchanged, significantly improving the binary
compatibility of the SDK dynamic library. Only the integrated modules in this
repository will be dynamically loaded during initialization, not user-defined
modules.

Due to GPL-3.0's definition regarding whether module code is part of the overall
program or a new program, GpgFrontend modules must also be released under the
GPL-3.0 or later license. Modules can freely utilize the capabilities of the Qt
libraries used by GpgFrontend, including Core, Widget, and Network. This means
that modules will only be loaded by GpgFrontend if they use the same version of
the Qt libraries.

Modules using an SDK version higher than what GpgFrontend supports will not be
loaded. The SDK version number corresponds directly to the GpgFrontend version
number. In the absence of comprehensive documentation, users can refer to the
implementation code of existing modules to develop their own modules if
necessary.

I am committed to adding more modules in the future, and any functionality not
directly related to the core focus of GpgFrontend will be developed as modules.
Unfortunately, due to time constraints, I cannot maintain both the Qt5 and Qt6
versions of the integrated modules. Therefore, the integrated modules in this
repository no longer support Qt5 and will not be released with the Qt5 version
of GpgFrontend. However, the Qt5 version of GpgFrontend still includes a
complete module system, so users can develop or port their own modules.

### Module Development

Compiling a module relies on the SDK header files, which then need to be linked
with the corresponding `libgpgfrontend_module_sdk` library (dynamic library).
Specific methods and project configuration can refer to the existing module
project configuration files in this repository. Currently, GpgFrontend provides
build options to compile and install the SDK, namely
`GPGFRONTEND_BUILD_TYPE_FULL_SDK` and `GPGFRONTEND_BUILD_TYPE_ONLY_SDK`.

After compiling the SDK with GpgFrontend, it usually includes the `include` and
`lib` folders. These can be directly installed or placed in the corresponding
version number directory under the `sdk` directory in this repository, such as
`sdk/2.1.3`. Then, you can run the feature configuration and compile the module
code in this repository.

Modules are released as single dynamic libraries. The file extension for dynamic
libraries varies across operating systems, so the code in this repository needs
to be able to compile into dynamic libraries on all operating systems supported
by GpgFrontend.

Introducing third-party libraries to integrated modules (other than those
already used by GpgFrontend) requires great caution and should only be done when
necessary. If the Qt library can provide the required functionality, make every
effort to use functions from the Qt library first. If a third-party library is
indeed needed, it must be installed as a dynamic library along with the module
itself in the correct directory.

User-developed module code does not need to be included in this repository.
Users can develop their own modules for personal use or limited distribution.
The advantage of placing your module in this repository is that it will be
released with GpgFrontend, making it available to a broader audience.
