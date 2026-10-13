# Installing pbnotes on the PocketBook InkPad One

These steps install the app from the release zip. They are written for the InkPad One
(RK3566); other models are untested.

## 1. Copy the files

1. Connect the reader over USB so its storage appears as a drive.
2. Unzip the package and copy the `applications/` folder into the **root** of the reader's
   storage, merging it with the existing `applications/` folder. You should end up with:

   ```text
   applications/pbnotes.app            launcher (shell script)
   applications/pbnotes.bin            the program
   applications/pbnotes-cacert.pem     bundled CA certificates
   applications/pbnotes-emoji.ttf      monochrome emoji font (Noto Emoji)
   applications/pbnotes-emoji-OFL.txt  license of the emoji font (SIL OFL 1.1)
   applications/pbnotes-licenses/      license texts shown by Settings > About (copy the whole folder)
   applications/icons/pbnotes.bmp
   applications/icons/pbnotes_f.bmp
   ```

## 2. Register the app in the Applications menu

The reader only lists apps that are registered in its desktop configuration.

1. On the reader's storage open `system/config/desktop/view.json`. **Back it up first.**
2. Under the top-level `"applications"` object add:

   ```json
   "U_PbNotes": {
     "path": "/mnt/ext1/applications/pbnotes.app",
     "title": "PB Notes",
     "icon": { "path": "applications/icons/pbnotes.bmp" },
     "focused_icon": { "path": "applications/icons/pbnotes_f.bmp" }
   }
   ```

3. Add `"U_PbNotes"` to one of the `"groups"` application lists, for example the one
   named `@General`.
4. Rules to keep in mind: the id must start with `U_`, `path` must be absolute, and the icons
   must be opaque BMP files (122x122, 8 bit).
5. Safely eject the reader and **reboot** it.

## 3. Start the app

Open Applications, then the group you added the entry to, and start **PB Notes**. The
launcher writes a log to `system/pbnotes/pbnotes.log`; the app's own data (the notes
database) lives in the same folder.

## Upgrading from an earlier version

Copy the new `applications/` files over the old ones. On the first start the app upgrades its
database in `system/pbnotes/` (schema version 2, for attachments); it keeps a backup during the upgrade and
deletes it once the check passed. **The upgrade is one way**: an older build of PB Notes refuses the
upgraded database, so keep a copy of `system/pbnotes/pbnotes.db` if you may want to go back.

Attachments are stored in `system/pbnotes/attachments/` (nothing is downloaded until you tap *Download*).
Each file is at most 10 MiB. The folder is cleaned automatically; do not put your own files there.

The *Attach* file browser starts at the folders `Audio Books`, `Downloads`, `Music`, `My pictures`, `Notes`, `Photo` and
`screens` on the reader's storage (those that exist); put the files you want to attach in one of them.

## Servers with a private or self-signed certificate

PB Notes always verifies the server's certificate and has no switch to skip this. If your server uses
a certificate from a private authority (or a self-signed one), give the app that certificate:

1. Get the certificate file in PEM format (a text file that starts with `-----BEGIN CERTIFICATE-----`).
   For a private authority use its root certificate; for a self-signed server certificate use that
   certificate itself (its subject alternative name must match the server's address).
2. Connect the reader over USB and copy the file to `system/pbnotes/custom-ca.pem` on the reader's
   storage (create the folder `pbnotes` inside `system` if it does not exist yet).
3. Safely eject the reader and start PB Notes again. **Settings** shows "Custom CA file: loaded" with
   the number of certificates. If it says "invalid", the file is ignored (it must contain only
   well-formed certificates and be smaller than 1 MB).

## Troubleshooting

- Nothing happens when starting: read `system/pbnotes/pbnotes.log` on the reader's storage (the launch before is in
  `pbnotes.log.prev`). If it says `failed to open device file /dev/mali0 ... Permission denied` (exit code 134), the
  reader's system has lost access to the graphics chip for new apps. PB Notes retries once by itself; if that fails,
  restart the reader. Other apps built the same way (InkFeed) are affected as well. Please keep both log files and
  note what you did before (how the app was left, whether a sync was running).
- The entry does not appear: check the JSON of `view.json` is still valid, then reboot.
