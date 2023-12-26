import qbs

Project {
    qbsSearchPaths: [ "qbs/" ]

    CppApplication {
        name: "Desktop Duplicator"
        targetName: "deskdupl"
        consoleApplication: false

        Depends { name: 'cpp' }
        cpp.cxxLanguageVersion: "c++23"
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

        Depends { name: 'hlsl' }
        hlsl.shaderModel: '4_0_level_9_3'

        Group {
            name: 'PCH'
            prefix: 'src/'
            files: ['stable.h']
            fileTags: ["cpp_pch_src"]
        }
        Group {
            name: 'Meta'
            prefix: 'src/meta/'
            files: [
                "array_view.h",
                "callback_adapter.h",
                "comptr.h",
                "flags.h",
                "member_method.h",
                "scope_guard.h",
            ]
        }
        Group {
            name: 'Win32'
            prefix: 'src/win32/'
            files: [
                "DisplayMonitor.cpp",
                "DisplayMonitor.h",
                "Dpi.h",
                "Geometry.h",
                "Geometry.ostream.h",
                "Handle.h",
                "PowerRequest.cpp",
                "PowerRequest.h",
                "Process.cpp",
                "Process.h",
                "TaskbarList.cpp",
                "TaskbarList.h",
                "Thread.cpp",
                "Thread.h",
                "ThreadLoop.cpp",
                "ThreadLoop.h",
                "WaitableTimer.cpp",
                "WaitableTimer.h",
                "Window.cpp",
                "Window.h",
                "Window.ostream.h",
                "WindowMessageHandler.h",
            ]
        }
        Group {
            name: 'Main'
            prefix: "src/"

            Group {
                name: 'Pixelshaders'
                hlsl.shaderType: 'ps'
                files: ["MaskedPixelShader.hlsl", "PlainPixelShader.hlsl"]
            }
            Group {
                name: 'Vertexshaders'
                hlsl.shaderType: 'vs'
                files: ["VertexShader.hlsl"]
            }
            Group {
                name: 'Capture'
                files: [
                    "CaptureThread.cpp",
                    "CaptureThread.h",
                    "CapturedUpdate.h",
                    "FrameContext.h",
                ]
            }
            Group {
                name: 'Output'
                files: [
                    "BaseRenderer.cpp",
                    "BaseRenderer.h",
                    "FrameUpdater.cpp",
                    "FrameUpdater.h",
                    "PointerUpdater.cpp",
                    "PointerUpdater.h",
                    "WindowRenderer.cpp",
                    "WindowRenderer.h",
                    "renderer.cpp",
                    "renderer.h",            ]
            }
            Group {
                name: 'Application'
                files: [
                    "CaptureAreaWindow.cpp",
                    "CaptureAreaWindow.h",
                    "DuplicationController.cpp",
                    "DuplicationController.h",
                    "MainApplication.cpp",
                    "MainApplication.h",
                    "MainController.h",
                    "MainThread.cpp",
                    "MainThread.h",
                    "Model.cpp",
                    "Model.h",
                    "OutputWindow.cpp",
                    "OutputWindow.h",
                    "RenderThread.cpp",
                    "RenderThread.h",
                    "TaskbarButtons.cpp",
                    "TaskbarButtons.h",
                ]
            }
            files: [
                "main.cpp",
                "main.ico",
                "main.manifest",
                "main.rc",
            ]
        }

        Group {
            name: "install"
            fileTagsFilter: "application"
            qbs.install: true
        }
    }

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
