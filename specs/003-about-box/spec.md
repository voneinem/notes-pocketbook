# Feature Specification: About Box (Version, Author, License, Third-Party Licenses)

**Feature Branch**: `003-about-box`

**Created**: 2026-10-11

**Status**: Draft

**Input**: User description: "About Box with Version, Author, License + Licenses of used libraries"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - See which version of the app I have (Priority: P1)

A reader opens the app's Settings and chooses "About". A screen shows the app's name, its version,
who wrote it, and the license it is published under. They can tell at a glance which build is
installed on the reader, for example to report a problem or to check whether an update took effect.
A Back control returns them to Settings.

**Why this priority**: Today there is nowhere in the app that says which version is installed. The
version, author and license are the core of an About box and are useful on their own, without the
third-party list.

**Independent Test**: Open Settings, choose About, and verify the app name, version, author and
license are shown and match the installed build; choose Back and verify Settings is shown again.

**Acceptance Scenarios**:

1. **Given** the app is running, **When** the reader opens Settings and chooses About, **Then** a
   screen shows the app name, the version of the installed build, the author, and the license name.
2. **Given** a build made from a released version, **When** the About screen is shown, **Then** the
   version is the one that build was released as, and it differs between two different releases.
3. **Given** a build whose version cannot be determined, **When** the About screen is shown,
   **Then** it shows an explicit "unknown" version instead of an empty value or an error.
4. **Given** the About screen is shown, **When** the reader chooses Back, **Then** Settings is
   shown again, in the state they left it.

---

### User Story 2 - Read the license of the app (Priority: P2)

A reader (or someone the reader lent the device to) wants to know what they may do with the app. On
the About screen they open the app's license and read its full text, paging through it with the
device's page buttons, then go back.

**Why this priority**: Showing only the license's name is a pointer; the license text is what grants
the rights. It is needed for the About box to be useful as a license notice, but the version and
author are valuable before it exists.

**Independent Test**: From the About screen open the app license and verify the full text is shown
and can be read to the end by paging; go back and verify the About screen is shown.

**Acceptance Scenarios**:

1. **Given** the About screen is shown, **When** the reader opens the app license, **Then** its full
   text, including the copyright line, is shown.
2. **Given** the license text is longer than one screen, **When** the reader uses the page buttons
   or the on-screen page controls, **Then** the text moves page by page and the reader can reach
   the last line.
3. **Given** the device has no network connection, **When** the reader opens the license, **Then**
   the text is shown in full (it does not depend on a connection).

---

### User Story 3 - See the licenses of the libraries and resources the app uses (Priority: P2)

A reader wants to know which third-party software the app is built on. On the About screen they
open "Third-party licenses" and see a list of every library, font, data file and code base the app
uses, each with its name, what it is used for, and its license. They open any entry and read that
component's full license text, with its copyright holder, and go back to the list.

**Why this priority**: The licenses of several components require that their notice and license
text travel with the app. Doing this inside the app means a reader who has only the device, and no
repository or download page, still receives them. It sits at the same priority as the app license
because both are license obligations, and it can be delivered independently of it.

**Independent Test**: Open Third-party licenses and verify that every component named in the
project's third-party notice file appears in the list with a license name; open each entry and
verify its full license text is shown; go back and verify the list is shown again.

**Acceptance Scenarios**:

1. **Given** the About screen is shown, **When** the reader opens Third-party licenses, **Then** a
   list shows every third-party component with its name, its purpose in a few words, and the name of
   its license.
2. **Given** the list is shown, **When** the reader opens an entry, **Then** the copyright notice
   and full license text of that component are shown, and can be paged to the end.
3. **Given** the list is longer than one screen, **When** the reader uses the page controls,
   **Then** they can reach every entry.
4. **Given** a component whose license text is not available offline (for example one that is only
   referred to by name and address), **When** the reader opens its entry, **Then** the entry shows
   the license name, the copyright holder and the address where the full license can be read,
   instead of an empty or broken screen.
5. **Given** the device has no network connection, **When** the reader opens the list and any
   entry, **Then** everything is shown without a connection.

---

### Edge Cases

- What happens when the reader rotates the device while an About screen is open? Layout adapts to
  the new orientation and the reader stays on the same screen, at the same position in the text.
- What happens when the author or license information cannot be loaded? The affected field shows an
  explicit "unavailable" marker; the rest of the About screen still works and the app does not
  crash.
- What happens when a license text is very long (for example a full legal text with many pages)?
  It is paged, and the reader can leave at any page with Back.
- What happens when the reader opens About while a sync is running? The About screen opens at once
  and the sync is neither interrupted nor delayed.
- What happens with characters outside plain English in names or notices (for example accented
  letters in a copyright holder's name)? They are shown correctly.
- What happens if the app is installed without its bundled license files? The About screen still
  shows version and author, and the missing license content is marked unavailable (see above)
  rather than blocking the screen.

## Requirements *(mandatory)*

### Functional Requirements

**About screen**

- **FR-001**: The app MUST provide an About screen that the reader can open from Settings, and
  leave with a Back control that returns to Settings.
- **FR-002**: The About screen MUST show the app's name, the version of the installed build, the
  author's name, and the name of the license the app is published under.
- **FR-003**: The version shown MUST be the version the installed build was released as, and MUST be
  the same version that the build identifies itself with elsewhere (for example in the install
  bundle or release notes). When it cannot be determined the screen MUST show "unknown" instead of an
  empty value.
- **FR-004**: The About screen MUST also show the copyright line of the app (year and holder), and
  MAY show a short one-line description of what the app is.
- **FR-005**: The About screen MUST be reachable and usable on the reader without a network
  connection and MUST NOT send any data anywhere (no update check, no usage reporting).

**App license**

- **FR-006**: The reader MUST be able to open the full text of the app's own license from the About
  screen, including its copyright line.
- **FR-007**: The license text MUST be readable by paging with the device's page buttons and with
  on-screen page controls, consistent with how other long content in the app is paged, and MUST be
  reachable to its last line.

**Third-party licenses**

- **FR-008**: The About screen MUST give access to a list of third-party licenses. The list MUST
  contain every third-party library, code base, font and data file that is shipped with the app or
  that the app loads at run time, including those provided by the device's system software.
- **FR-009**: Each entry in the list MUST show the component's name, a short statement of what the
  app uses it for, and the name of its license.
- **FR-010**: The reader MUST be able to open an entry and read that component's copyright holder
  and full license text, and go back to the list. Where a license's full text is not distributed with
  the component (for example a public-domain dedication), the entry MUST state this and give the
  license or dedication name and the address where it is published.
- **FR-011**: Each license text MUST be readable by paging, as in FR-007, and the list itself MUST be
  fully reachable by paging when it is longer than one screen.
- **FR-012**: The third-party list shown in the app MUST agree with the project's third-party notice
  file: every component named there MUST be in the app, and no component MUST appear in the app that
  is not in the notice file. There MUST be one place where a component is declared, from which both
  are produced or against which both are checked, so they cannot silently drift apart.
- **FR-013**: Adding a third-party component to the app without adding it to the list MUST be caught
  before a release (a check fails), not left to memory.

**Quality and robustness**

- **FR-014**: The About, license and third-party screens MUST follow the app's existing e-ink
  conventions: high-contrast black on white, large readable text, flat buttons, no animations, and
  paged scrolling rather than kinetic scrolling.
- **FR-015**: The screens MUST work in both portrait and landscape and MUST keep the reader's
  position when the device is rotated.
- **FR-016**: If a license file or the author information is missing or unreadable, the affected
  item MUST be marked unavailable, and the rest of the About screen MUST remain usable; the app MUST
  NOT crash or show an empty screen.
- **FR-017**: Opening these screens MUST NOT change any notes, settings, account data or sync state.

### Key Entities

- **App Information**: the app's name, the version of the installed build, the author, the copyright
  line and the license it is published under. Read-only; one per installed build.
- **App License**: the full text of the app's own license, with its copyright line. Read-only.
- **Third-Party Component**: a library, code base, font or data file the app uses. Has a name, a
  short purpose, a copyright holder, a license name, and either the full license text or the
  address where it is published.
- **Third-Party License List**: the ordered set of all Third-Party Components, kept in agreement with
  the project's third-party notice file.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A reader can find the installed version number from the notes list in no more than 3
  taps (Settings, About) and under 10 seconds.
- **SC-002**: 100% of third-party components named in the project's third-party notice file appear
  in the in-app list with the correct license name, verified by an automated check on every build.
- **SC-003**: A reader can open the full license text of any listed component in no more than 2
  taps from the About screen, and read it to the last line, with the device offline.
- **SC-004**: For every release, the version shown in the About screen equals the release's version
  identifier (0 mismatches over the release checklist).
- **SC-005**: Opening or leaving any About screen causes no change to stored notes, settings, or
  sync state (verified before and after opening in a test), and no network traffic.
- **SC-006**: No About screen shows an empty body or an error for a missing license item: each
  missing item shows an explicit "unavailable" message in 100% of the tested missing-file cases.

## Assumptions

- The author is Volker von Einem and the app is published under the MIT License, as stated in the
  repository's license file. The About screen shows the same copyright line as that file.
- The app's name shown is the product name used elsewhere in the app and its documentation.
- The About screen is reached from Settings, the existing place for app-level information, rather
  than adding a new item to the notes list; Settings gets one new About entry.
- The "third-party components" are those listed in the project's third-party notice file today
  (InkFeed-derived code, the Mozilla CA certificate bundle, SQLite, and the Noto Emoji font), plus
  the libraries the app loads from the device's system software that the notice file does not yet
  name (for example the UI toolkit, the network library, the TLS library, and the PocketBook SDK
  library). Planning will verify the exact set and the license of each, and the notice file will be
  extended to match (FR-012).
- A component is listed when it is compiled into or shipped with the app, named as needed by the
  app's program file, or performs a function the app relies on without being linked by it (the
  TLS library). The device's wider library set is not listed (decided in planning).
- The version shown is the build's version identifier; until a release is tagged this is a commit
  identifier.
- Components that come from the device's system software are listed because the app uses them, but
  their license texts are those published by their projects; where the device does not carry a
  copy, the entry gives the license name, holder and published address (FR-010).
- The English UI text is the baseline; the About screens use the same translation mechanism as the
  rest of the app, but the license texts themselves are shown in their original language.
- There is no in-app link to a website, no update check, no "send feedback" and no changelog in this
  feature. Opening an address in the browser, if the reader wants one, can reuse the link handling
  from the rich-note-content feature later and is out of scope here.
- Any text the reader may copy, share or export from the About screens is out of scope; the screens
  are for reading only.
