# AAX SDK

Drop Avid's AAX SDK zip here to build the Pro Tools version of the plugin:

```
AAX/aax-sdk-2-9-0.zip
```

Download it from your Avid developer account (<https://developer.avid.com>).
It is Avid's, not ours, and may not be redistributed — hence the zip and the
directory it unpacks to are both gitignored, and only this note is committed.

That is the entire setup. `scripts/aax-sdk.sh` (`scripts/aax-sdk.ps1` on
Windows) unpacks the zip beside itself the first time anything needs it, and
`install-local.sh`, `dist-macos.sh`, `dist-macos-pkg.sh` and
`dist-windows.ps1` then build and package the AAX plugin alongside the other
formats. With no zip here they build AU, VST3 and Standalone as usual — an
absent SDK is not an error, and `--no-aax` (`-NoAax` on Windows) skips AAX
even when it is present.

One thing this does *not* do: Pro Tools refuses to load any AAX plugin that
has not been PACE-signed with Avid's `wraptool`, which needs a developer
account and a signing certificate. See the **AAX (Pro Tools)** section of
[RELEASE.md](../RELEASE.md#aax-pro-tools) for where that step goes.
