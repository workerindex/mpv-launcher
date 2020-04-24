# sudo-mpv-launcher

Provides a `sudo-mpv-launcher://` protocol handler for Windows. Launches mpv from browsers with **arbitrary arguments**.

**WARNING: Since it allows arbitrary arguments, NO ONE SHOULD USE IT WITHOUT KNOWING WHAT IT MEANS AND WHAT HE/SHE IS DOING!**

# Protocol Scheme Encoding

```typescript
interface MpvLauncherScheme {
  player: "mpv"; // currently only mpv is supported
  args: string[]; // array of command line arguments append to the player executable
}

function encodeMpvLaucherScheme(scheme: MpvLauncherScheme): string {
  // Serialize in JSON format
  const json = JSON.stringify(scheme);

  // Base64-RAW-URL Encode
  const base64 = btoa(json)
    .replace(/\+/g, "-")
    .replace(/\//g, "_")
    .replace(/=/g, "");

  // Append to protocol
  return "sudo-mpv-launcher://" + base64;
}
```
