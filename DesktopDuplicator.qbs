Project {
    qbsSearchPaths: [ "qbs/" ]

    CppApplication {
        name: "Desktop Duplicator"
        targetName: "deskdupl"
        consoleApplication: false

        Depends { name: "cpp" }
        cpp.cxxLanguageVersion: "c++23"
        cpp.treatWarningsAsErrors: true
        cpp.enableRtti: false // disable runtime type information for faster build and smaller object files and executable

        cpp.minimumWindowsVersion: "10.0"
        cpp.dynamicLibraries: ["d3d11", "User32", "Gdi32", "Shell32", "Ole32", "Comctl32"]
        cpp.includePaths: ["src"]

        Properties {
            condition: qbs.toolchainType == "msvc"
            cpp.generateManifestFile: false
            cpp.defines: ["NOMINMAX"]
            cpp.cxxFlags: [
                "/analyze", "/Zc:char8_t-",
                "/permissive-", "/Zc:__cplusplus", // best C++ compatibilty
                "/Zc:inline", // do not include inline code in object files
                "/Zc:throwingNew", // avoid redundant null checks after new
                "/diagnostics:caret", // better error postions
                "/W4", // enable all warnings
                "/experimental:external", "/external:anglebrackets", "/external:W0" // ignore warnings from external headers
            ]
        }
        Properties {
            condition: qbs.toolchainType == "clang-cl"
            cpp.generateManifestFile: false
            cpp.defines: ["NOMINMAX"]
            cpp.cxxFlags: [
                "/permissive-", "/Zc:__cplusplus", // best C++ compatibilty
                "/Zc:inline", // do not include inline code in object files
                "/diagnostics:caret", // better error postions
                "/W4", // enable all warnings
            ]
        }
        Properties {
            condition: qbs.toolchain.contains("clang")
            // note: would require "--target=x86_64-pc-windows-msvc19.43.34808" to work
            // useful: to run third party clang based tools
            cpp.defines: [
                "_M_X64=100",
                "_M_AMD64=100",
                "_WIN64=1",
                "NOMINMAX"
            ]
            cpp.cxxFlags: [
                "-fms-volatile",
                "-fms-extensions",
                "-fms-compatibility-version=19.43.34808",
                "-fms-compatibility",
                // "-###" // print cc1 subcommand arguments
            ]
            cpp.systemIncludePaths: [
                // note: actually needs msvc include & windows kit includes
                "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.43.34808/include",
                "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.43.34808/atlmfc/include",
                "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/VS/include",
                "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/shared",
                "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/um",
                "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/winrt",
                "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/ucrt"
            ]
        }

        Depends { name: "hlsl" }
        hlsl.shaderModel: "4_0_level_9_3"

        Group {
            name: "PCH"
            prefix: "src/"
            files: ["stable.h"]
            fileTags: ["cpp_pch_src"]
        }
        Group {
            name: "Meta"
            prefix: "src/meta/"
            files: [
                "callback_adapter.h",
                "comptr.h",
                "flags.h",
                "fromByteSpan.h",
                "member_method.h",
                "scope_guard.h",
            ]
        }
        Group {
            name: "Win32"
            prefix: "src/win32/"
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
            name: "Main"
            prefix: "src/"

            Group {
                name: "Pixelshaders"
                hlsl.shaderType: "ps"
                files: ["MaskedPixelShader.hlsl", "PlainPixelShader.hlsl"]
            }
            Group {
                name: "Vertexshaders"
                hlsl.shaderType: "vs"
                files: ["VertexShader.hlsl"]
            }
            Group {
                name: "Capture"
                files: [
                    "CaptureThread.cpp",
                    "CaptureThread.h",
                    "CapturedUpdate.h",
                    "FrameContext.h",
                ]
            }
            Group {
                name: "Output"
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
                    "renderer.h",
                ]
            }
            Group {
                name: "Application"
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
