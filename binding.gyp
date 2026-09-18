{
  "variables": {
    "vcpkg_root%": "<!(node scripts/require-vcpkg-root.js)",
    "vcpkg_triplet%": "<!(node scripts/vcpkg-triplet.js)"
  },
  "targets": [
    {
      "target_name": "baja_xlsx",
      "sources": [
        "src/addon.cpp",
        "src/xlsx_reader.cpp",
        "src/xlsx_writer.cpp",
        "src/xlsx_patch.cpp",
        "src/xlsx_template.cpp",
        "src/write_snapshot.cpp",
        "src/image_extractor.cpp",
        "src/path_util.cpp",
        "src/zip_reader.cpp",
        "src/zip_writer.cpp",
        "src/xml_parsers.cpp"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "<(module_root_dir)/src",
        "<(vcpkg_root)/installed/<(vcpkg_triplet)/include"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "defines": [
        "NAPI_DISABLE_CPP_EXCEPTIONS"
      ],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "conditions": [
        [
          "OS=='win'",
          {
            "defines": [
              "_HAS_EXCEPTIONS=1"
            ],
            "msvs_settings": {
              "VCCLCompilerTool": {
                "ExceptionHandling": 1,
                "AdditionalOptions": ["/std:c++17"]
              }
            },
            "libraries": [
              "<(vcpkg_root)/installed/<(vcpkg_triplet)/lib/xlnt.lib",
              "<(vcpkg_root)/installed/<(vcpkg_triplet)/lib/zip.lib",
              "<(vcpkg_root)/installed/<(vcpkg_triplet)/lib/z.lib"
            ]
          }
        ],
        [
          "OS=='linux'",
          {
            "cflags_cc": [
              "-std=c++17",
              "-fexceptions"
            ],
            "libraries": [
              "<!@(node scripts/vcpkg-link-flags.js)"
            ]
          }
        ],
        [
          "OS=='mac'",
          {
            "xcode_settings": {
              "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
              "CLANG_CXX_LIBRARY": "libc++",
              "MACOSX_DEPLOYMENT_TARGET": "10.15",
              "OTHER_CPLUSPLUSFLAGS": ["-std=c++17"]
            },
            "libraries": [
              "<!@(node scripts/vcpkg-link-flags.js)"
            ]
          }
        ]
      ]
    }
  ]
}
