/*
 * FLUG-OS — Version {-1, 0, +1}
 * Semantic versioning + git commit embedding.
 *
 * Version scheme: MAJOR.MINOR.PATCH (semver)
 *   MAJOR: breaking protocol changes
 *   MINOR: new features (wave modes, channels, filters)
 *   PATCH: bugfixes, performance, documentation
 *
 * Build metadata appended automatically by CI:
 *   v0.2.0+3d32bd9  → tagged release
 *   v0.2.0-dev       → development build
 */

#ifndef FLUGOS_VERSION_H
#define FLUGOS_VERSION_H

#define FLUGOS_MAJOR    0
#define FLUGOS_MINOR    2
#define FLUGOS_PATCH    0

#define FLUGOS_VERSION_STR  "v" STR(FLUGOS_MAJOR) "." STR(FLUGOS_MINOR) "." STR(FLUGOS_PATCH)

#define STR(x)  _STR(x)
#define _STR(x) #x

// Git commit hash injected by CI build.
// Falls back to "dev" for local builds.
#ifndef FLUGOS_COMMIT
#define FLUGOS_COMMIT "dev"
#endif

// Full version string for boot screen and UART output
#define FLUGOS_FULL_VERSION  FLUGOS_VERSION_STR "+" FLUGOS_COMMIT

#endif
