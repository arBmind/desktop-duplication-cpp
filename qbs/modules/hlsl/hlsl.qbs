import qbs 1.0
import qbs.File
import qbs.FileInfo
import qbs.TextFile
import qbs.WindowsUtils
import qbs.ModUtils
import qbs.Utilities
import qbs.Process


Module {
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
            var cmd = new JavaScriptCommand();
            cmd.args = args;
            cmd.outputPath = output.filePath;
            cmd.hlslPath = ModUtils.moduleProperty(product, "hlslPath");
            cmd.sourceCode = function() {
                var process = new Process();
                process.exec(hlslPath, args, true);
                var file = new TextFile(outputPath, TextFile.ReadWrite);
                var content = file.readAll();
                file.truncate();
                file.writeLine("#pragma once");
                file.writeLine("#include <windows.h>");
                file.write(content);
                file.close();
                var output = process.readStdOut();
                var lines = output.split("\r\n").filter(function (s) {
                    return !s.endsWith(inputFileName);
                });
                console.info(lines.join("\r\n"));
            };
            cmd.description = "compiling shader " + input.fileName;
            cmd.inputFileName = input.fileName;
            return cmd;
        }
    }
}
