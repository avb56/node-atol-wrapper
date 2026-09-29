{
  # The driver (ATOL DTO 10) is not linked and not copied into the build: it is
  # loaded at run time from where it is installed on the machine — see
  # src/fptr_api.h for the search order. libfptr10.h in src/ is used only for
  # types and constants, so the addon builds without any driver binaries.
  "targets": [
    {
      "target_name": "node_atol_wrapper",
      "include_dirs" : [
        "src",
        "<!(node -e \"require('nan')\")"
      ],
      "sources": [
        "src/index.cc",
        "src/fptr10.cc",
        "src/fptr_api.cc",
        "src/utils.cc",
        "src/json_worker.cc"
      ],
      "conditions":[
        ["OS=='linux'", {
          "link_settings": {
            # dlopen/dlsym: part of libc since glibc 2.34, a separate libdl before.
            "libraries": ["-ldl"]
          }
        }],
        ["OS=='win'", {
          "link_settings": {
            # RegGetValueW — reading the driver installation directory.
            "libraries": ["advapi32.lib"]
          }
        }],
        ["OS=='mac'", {
          "xcode_settings": {
            "OTHER_CPLUSPLUSFLAGS": [
              "-std=c++17",
              "-stdlib=libc++"
            ],
            "OTHER_LDFLAGS": [
              "-stdlib=libc++"
            ],
            "MACOSX_DEPLOYMENT_TARGET": "10.15"
          }
        }]
      ]
    }
  ]
}
