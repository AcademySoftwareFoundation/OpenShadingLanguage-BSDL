# Security Policy

## Supported Versions

This gives guidance about which branches are supported with patches to
security vulnerabilities.

| Version / branch  | Supported                                            |
| --------- | ---------------------------------------------------- |
| main      | :white_check_mark: :construction: ALL fixes immediately, but this is a branch under development with a frequently unstable ABI and occasionally unstable API. |


## Reporting a Vulnerability

If you think you've found a potential vulnerability in BSDL, please
report it to the maintainers. Include detailed steps to reproduce the issue,
and any other information that could aid an investigation.

The best way to report a vulnerability is to file a GitHub [security
advisory](https://github.com/AcademySoftwareFoundation/OpenShadingLanguage-BSDL/security/advisories/new).
If that is not possible, it is also fine to email your report to
security@openshadinglanguage.org. Only the project administrators have access
to these reports.

Our policy is to respond to vulnerability reports within 14 days, and to
address critical security vulnerabilities rapidly and post patches as quickly
as possible.


## What do we consider a vulnerability?

We only consider a situation to be a security vulnerability if an untrusted
party can plausibly trigger the flaw through normal product inputs (for
example, a maliciously crafted file that might compromise a renderer when
loaded). We do not support requesting a CVE for API-only or caller-controlled
failures with no realistic adversarial path.

BSDL is a computation library API only and does not directly read untrusted
input files, so while we will try to quickly fix bugs, the we expect that it
will be very unusual for anything in this project to qualify as a "security
vulnerability" according to the above definition, and therefore require a
formal CVE.

Flaws whose root cause lies in a dependency should be reported and fixed
upstream; the upstream project owns the CVE when one is warranted.


## Other security features

### Signed tags

When we start doing real releases, we intend to cryptographically sign release
tags, as we do in the main OSL project.

To verify a tag, you can use the `git tag -v` command, which will check
the signature against the public key that is included in the repository.
For example,

```bash
git tag -v v1.14.3.0
```

## Outstanding Security Issues

None known


## History of CVE Fixes

None to date
