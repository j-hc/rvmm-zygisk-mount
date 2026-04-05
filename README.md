# rvmm-zygisk-mount

Injects the mounts at the pre app specialization with Zygisk (for [revanced-magisk-module](https://github.com/j-hc/revanced-magisk-module)).

Fixes problems such as,
- "Reflash needed" error after reboots
- "Suspicious mount detected" warnings from root detector apps

If you are using revanced-magisk-module from rebanders, who completely remove my name from the modules, rvmm-zygisk-mount wont work.

## Usage
- Make sure the modules of whichever app you want is installed from revanced-magisk-module
- Flash rvmm-zygisk-mount module. It will automatically take care of everything
- **Do not** put the patched apps you want in the denylist

#### You do not need to use this module. The classic mount method of revanced-magisk-module is just fine for many people.
#### Use this only if you need to, or you are having trouble with the classic mount method.

