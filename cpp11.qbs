import qbs

Project {
    qbsSearchPaths: [ "qbs/" ]

    CppApplication {
        name: "desktop-duplication"
        consoleApplication: false

        Depends { name: 'cpp' }
        cpp.cxxLanguageVersion: "c++2a"
        cpp.minimumWindowsVersion: "10.0"
        cpp.generateManifestFile: false
        cpp.includePaths: 'src'
        cpp.cxxFlags: ['/analyze']
        cpp.dynamicLibraries: ['d3d11', "User32", "Gdi32", "Shell32", "Ole32", "Comctl32"]

        Depends { name: 'Microsoft.GSL' }

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
            name: 'PCH'
            files: ["src/stable.h"]
            fileTags: ["cpp_pch_src"]
        }

        files: [
            "src/BaseRenderer.cpp",
            "src/BaseRenderer.h",
            "src/CaptureThread.cpp",
            "src/CaptureThread.h",
            "src/CapturedUpdate.h",
            "src/FrameContext.h",
            "src/FrameUpdater.cpp",
            "src/FrameUpdater.h",
            "src/PointerUpdater.cpp",
            "src/PointerUpdater.h",
            "src/WindowRenderer.cpp",
            "src/WindowRenderer.h",
            "src/application.cpp",
            "src/application.h",
            "src/main.cpp",
            "src/main.ico",
            "src/main.manifest",
            "src/main.rc",
            "src/meta/array_view.h",
            "src/meta/comptr.h",
            "src/meta/flags.h",
            "src/meta/scope_guard.h",
            "src/meta/tuple.h",
            "src/renderer.cpp",
            "src/renderer.h",
            "src/win32/PowerRequest.cpp",
            "src/win32/PowerRequest.h",
            "src/win32/TaskbarList.cpp",
            "src/win32/TaskbarList.h",
        ]
    }

    Project {
        name: "Third Party"

        Product {
            name: "Microsoft.GSL"
            Export {
                 Depends { name: 'cpp' }
                 cpp.includePaths: 'packages/Microsoft.Gsl.0.1.2.1/build/native/include'
            }
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
            "qbs/modules/hlsl/hlsl.qbs",
        ]
    }
}
