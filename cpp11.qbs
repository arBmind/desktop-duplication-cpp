import qbs

Project {
    minimumQbsVersion: "1.6"

    qbsSearchPaths: [ "qbs/" ]

    CppApplication {
        name: "desktop-duplication"
        consoleApplication: false
        cpp.cxxLanguageVersion: "c++17"
        cpp.minimumWindowsVersion: "10.0"
        cpp.generateManifestFile: false
        cpp.includePaths: 'src'
        cpp.cxxFlags: ['/std:c++latest']
        cpp.dynamicLibraries: ['d3d11.lib', "user32.lib", "gdi32.lib"]

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

        files: [
            ".editorconfig",
            ".gitignore",
            "LICENSE",
            "README.adoc",
            "qbs/modules/hlsl/hlsl.qbs",
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
            "src/main.manifest",
            "src/main.rc",
            "src/meta/array.h",
            "src/meta/array_view.h",
            "src/meta/bits.h",
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
            "src/stable.h",
            "src/window_renderer.cpp",
            "src/window_renderer.h",
        ]
    }
}
