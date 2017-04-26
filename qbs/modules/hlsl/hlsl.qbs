import qbs 1.0
import qbs.File
import qbs.FileInfo
import qbs.WindowsUtils
import qbs.ModUtils
import qbs.Utilities

Module {
    id: module

    property string hlslName: "fxc.exe"
    property string hlslPath: hlslName
    property string outputDir: "hlsl"

    property stringList flags
    PropertyOptions {
        name: "flags"
        description: "additional flags"
    }

    property string entryPoint: 'main'
    PropertyOptions {
        name: "entryPoint"
        description: "name of entry point function"
    }

    property string shaderType // ps / vs / ...
    PropertyOptions {
        name: "shaderType"
        description: "type of shader to be compiled"
    }

    property string shaderModel // 1_0 / 2_0 / ...
    PropertyOptions {
        name: "shaderModel"
        description: "model level of the shader"
    }

    property string outVariableName: "g_*"
    PropertyOptions {
        name: "outVariableName"
        description: "name of the output variable"
    }

    Depends { name: "cpp" }
    cpp.includePaths: product.buildDirectory + '/' + outputDir

    FileTagger {
        patterns: "*.hlsl"
        fileTags: ["hlsl"]
    }

    Rule {
        inputs: ["hlsl"]
        Artifact {
            filePath: ModUtils.moduleProperty(product, "outputDir") + '/' + input.completeBaseName + ".h"
            fileTags: ["hpp"]
        }
        prepare: {
            var args = ["/nologo",
                        '/E'+ModUtils.moduleProperty(input, 'entryPoint', 'hlsl'),
                        '/Vn'+ModUtils.moduleProperty(input, 'outVariableName', 'hlsl').replace('*',input.completeBaseName),
                        '/T'+ModUtils.moduleProperty(input, 'shaderType', 'hlsl') + '_' + ModUtils.moduleProperty(input, 'shaderModel', 'hlsl'),
                        "/Fh" + FileInfo.toWindowsSeparators(output.filePath)];

            if (ModUtils.moduleProperty(product, "debugInformation")) {
                args.push("/Zi");
                args.push("/Od");
            }
            args = args.concat(ModUtils.moduleProperty(input, 'flags', 'hlsl'));
            args.push(FileInfo.toWindowsSeparators(input.filePath));
            var cmd = new Command(ModUtils.moduleProperty(product, "hlslPath"), args);
            cmd.description = "compiling shader " + input.fileName;
            cmd.inputFileName = input.fileName;
            cmd.stdoutFilterFunction = function(output) {
                var lines = output.split("\r\n").filter(function (s) {
                    return !s.endsWith(inputFileName); });
                return lines.join("\r\n");
            };
            return cmd;
        }
    }
}
