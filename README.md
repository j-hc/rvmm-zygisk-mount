# rvmm-zygisk-mount

Injects the mounts at the pre app specialization with Zygisk (for [revanced-magisk-module](https://github.com/j-hc/revanced-magisk-module)).

- Supports Magisk and KernelSU
- For now, it only supports revanced-magisk-module but can be generalized

Fixes problems such as,
- "Reflash needed" error after reboots
- "Suspicious mount detected" warnings from root detector apps

## Usage
- Make sure the modules of whichever app you want is installed from [revanced-magisk-module](revanced-magisk-module)
- Flash rvmm-zygisk-mount module. It will automatically take care of everything
- ! **Do not** put the patched apps you want in the denylist !

#### You do not need to use this module. The classic mount method of revanced-magisk-module is just fine for many people.
#### Use this only if you need to, or you are having trouble with the classic mount method.
#### Keep in mind that this does not require xposed, but still has the overhead of the Zygisk injection which is a lot lighter than xposed.

