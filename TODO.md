
## Future Improvements

### Secure Activation Storage
Currently activation state is stored in a plain JSON file at `AppData/BeatConnect/<pluginId>/activation.json`. This is brittle - if deleted, user must re-activate.

**Consider:**
- Store in OS Credential Manager (Windows) / Keychain (macOS)
- Encrypted file with machine-bound key
- Hybrid: keychain as primary, JSON as fast-read cache

**Priority:** Low (current approach works, just not ideal)
