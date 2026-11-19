// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The app's version, semver. The one place it is set: scripts read it from
// here (scripts/release-app.sh), and git tags are v<this>. Bump it with an
// entry in CHANGELOG.md. The -zh.N suffix marks this fork's builds: the
// number before it is the upstream version it is based on.
#define PS5LM_VERSION "0.11.0-zh.1"
