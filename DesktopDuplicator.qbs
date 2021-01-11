import qbs

Project {
    qbsSearchPaths: [ "qbs/" ]

    CppApplication {
        name: "Desktop Duplicator"
        targetName: "deskdupl"
        consoleApplication: false

        Depends { name: 'cpp' }
        cpp.cxxLanguageVersion: "c++2a"
        cpp.treatWarningsAsErrors: true
        cpp.enableRtti: false // disable runtime type information for faster build and smaller object files and executable

        cpp.minimumWindowsVersion: "10.0"
        cpp.generateManifestFile: false
        cpp.includePaths: 'src'
        cpp.cxxFlags: ['/analyze', '/Zc:char8_t-']
        cpp.defines: ['NOMINMAX']
        cpp.dynamicLibraries: ['d3d11', "User32", "Gdi32", "Shell32", "Ole32", "Comctl32"]

        Properties {
            condition: qbs.toolchain.contains('msvc')
            cpp.cxxFlags: outer.concat(
                "/permissive-", "/Zc:__cplusplus", // best C++ compatibilty
                "/Zc:inline", // do not include inline code in object files
                "/Zc:throwingNew", // avoid redundant null checks after new
                "/diagnostics:caret", // better error postions
                "/W4", // enable all warnings
                "/experimental:external", "/external:anglebrackets", "/external:W0" // ignore warnings from external headers
            )
        }

        // Depends { name: 'Microsoft.GSL' }

        Depends { name: 'hlsl' }
        hlsl.shaderModel: '4_0_level_9_3'

        Group {
            name: 'Pixelshaders'
            hlsl.shaderType: 'ps'
            files: ["src/MaskedPixelShader.hlsl", "src/PlainPixelShader.hlsl"]
        }
        Group {
            name: 'Vertexshaders'
            hlsl.shaderType: 'vs'
            files: ["src/VertexShader.hlsl"]
        }
        Group {
            name: 'Capture'
            files: [
                "src/CaptureThread.cpp",
                "src/CaptureThread.h",
                "src/CapturedUpdate.h",
                "src/FrameContext.h",
            ]
        }
        Group {
            name: 'Output'
            files: [
                "src/BaseRenderer.cpp",
                "src/BaseRenderer.h",
                "src/FrameUpdater.cpp",
                "src/FrameUpdater.h",
                "src/PointerUpdater.cpp",
                "src/PointerUpdater.h",
                "src/WindowRenderer.cpp",
                "src/WindowRenderer.h",
                "src/renderer.cpp",
                "src/renderer.h",            ]
        }

        files: [
            "src/CaptureAreaWindow.cpp",
            "src/CaptureAreaWindow.h",
            "src/DuplicationController.cpp",
            "src/DuplicationController.h",
            "src/MainApplication.cpp",
            "src/MainApplication.h",
            "src/Model.cpp",
            "src/Model.h",
            "src/OutputWindow.cpp",
            "src/OutputWindow.h",
            "src/TaskbarButtons.cpp",
            "src/TaskbarButtons.h",
            "src/main.cpp",
            "src/main.ico",
            "src/main.manifest",
            "src/main.rc",
            "src/meta/array_view.h",
            "src/meta/comptr.h",
            "src/meta/flags.h",
            "src/meta/member_method.h",
            "src/meta/scope_guard.h",
            "src/win32/DisplayMonitor.cpp",
            "src/win32/DisplayMonitor.h",
            "src/win32/Dpi.h",
            "src/win32/Geometry.h",
            "src/win32/Geometry.ostream.h",
            "src/win32/Handle.h",
            "src/win32/MessageHandler.h",
            "src/win32/PowerRequest.cpp",
            "src/win32/PowerRequest.h",
            "src/win32/Process.cpp",
            "src/win32/Process.h",
            "src/win32/TaskbarList.cpp",
            "src/win32/TaskbarList.h",
            "src/win32/Thread.cpp",
            "src/win32/Thread.h",
            "src/win32/ThreadLoop.cpp",
            "src/win32/ThreadLoop.h",
            "src/win32/WaitableTimer.cpp",
            "src/win32/WaitableTimer.h",
            "src/win32/Window.cpp",
            "src/win32/Window.h",
            "src/win32/Window.ostream.h",
        ]

        Group {
            name: "install"
            fileTagsFilter: "application"
            qbs.install: true
        }
    }

    // Project {
    //     name: "Third Party"

    //     Product {
    //         name: "Microsoft.GSL"
    //         Export {
    //              Depends { name: 'cpp' }
    //              cpp.includePaths: 'packages/Microsoft.Gsl.0.1.2.1/build/native/include'
    //         }
    //     }
    // }

    Product {
        name: "Extra Files"
        builtByDefault: false

        files: [
            ".clang-format",
            ".editorconfig",
            ".gitignore",
            "LICENSE",
            "README.adoc",
        ]
    }
}
