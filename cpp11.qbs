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
            "src/application.cpp",
            "src/application.h",
            "src/base_renderer.cpp",
            "src/base_renderer.h",
            "src/capture_thread.cpp",
            "src/capture_thread.h",
            "src/captured_update.h",
            "src/frame_context.h",
            "src/frame_updater.cpp",
            "src/frame_updater.h",
            "src/main.cpp",
            "src/main.ico",
            "src/main.manifest",
            "src/main.rc",
            "src/meta/array.h",
            "src/meta/array_view.h",
            "src/meta/comptr.h",
            "src/meta/flags.h",
            "src/meta/opaque_value.h",
            "src/meta/optional.h",
            "src/meta/result.h",
            "src/meta/scope_guard.h",
            "src/meta/string_view.h",
            "src/meta/tuple.h",
            "src/pointer_updater.cpp",
            "src/pointer_updater.h",
            "src/renderer.cpp",
            "src/renderer.h",
            "src/win32/power_request.cpp",
            "src/win32/power_request.h",
            "src/win32/taskbar_list.cpp",
            "src/win32/taskbar_list.h",
            "src/window_renderer.cpp",
            "src/window_renderer.h",
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
