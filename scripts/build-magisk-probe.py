#!/usr/bin/env python3
"""Build a deterministic, local-only module lifecycle acceptance ZIP."""
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

marker = "arm13-module-lifecycle-20261124"
files = {
    "module.prop": "id=arm13_module_probe\nname=ARM13 Module Lifecycle Probe\nversion=1.0\nversionCode=1\nauthor=ARM13 lab\ndescription=Temporary local acceptance probe: marker, scripts, property and systemless file.\n",
    "customize.sh": 'ui_print "ARM13 self-test only: no policy or kernel changes"\nset_perm_recursive "$MODPATH" 0 0 0755 0644\nset_perm "$MODPATH/post-fs-data.sh" 0 0 0755\nset_perm "$MODPATH/service.sh" 0 0 0755\nset_perm "$MODPATH/uninstall.sh" 0 0 0755\nset_perm "$MODPATH/action.sh" 0 0 0755\n',
    "post-fs-data.sh": "#!/system/bin/sh\nmkdir -p /data/adb/arm13-module-probe\necho post-fs-data >> /data/adb/arm13-module-probe/events\n",
    "service.sh": "#!/system/bin/sh\necho service >> /data/adb/arm13-module-probe/events\ngetprop arm13.module.probe > /data/adb/arm13-module-probe/service-property\n",
    "uninstall.sh": "#!/system/bin/sh\necho uninstall >> /data/adb/arm13-module-probe/events\n",
    "action.sh": "#!/system/bin/sh\necho action >> /data/adb/arm13-module-probe/events\necho ARM13_MODULE_ACTION_PASS\n",
    "system.prop": f"arm13.module.probe={marker}\n",
    "system/etc/arm13-module-probe.txt": marker + "\n",
}
output = Path(__file__).resolve().parents[1] / "out/probes/arm13-module-probe.zip"
output.parent.mkdir(parents=True, exist_ok=True)
with ZipFile(output, "w", compression=ZIP_DEFLATED) as archive:
    for name, content in sorted(files.items()):
        info = ZipInfo(name, (2026, 11, 24, 0, 0, 0))
        info.compress_type = ZIP_DEFLATED
        info.external_attr = (0o100755 if name.endswith(".sh") else 0o100644) << 16
        archive.writestr(info, content)
print(output)
