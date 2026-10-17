# Conventional Commits 1.0.0

The commit message should be structured as follows:

```
<type>[optional scope]: <description>

[optional body]

[optional footer(s)]
```

## Types

| Type   | Meaning                              | SemVer  |
|--------|--------------------------------------|---------|
| `fix`  | A bug fix                            | PATCH   |
| `feat` | A new feature                        | MINOR   |
| `!`    | Breaking change (prefix or footer)   | MAJOR   |

Other accepted types: `build`, `chore`, `ci`, `docs`, `style`, `refactor`, `perf`, `test`, `revert`.

## Specification — Key Rules

1. Commits **MUST** be prefixed with a type (`feat`, `fix`, etc.) followed by an optional scope in `()`, optional `!`, and a required colon and space.
2. A **description** (short summary) **MUST** immediately follow the colon/space.
3. A **body** (longer description) **MAY** follow — it must begin one blank line after the description.
4. **Footers** (e.g. `BREAKING CHANGE:`, `Refs:`, `Closes:`) **MAY** follow — one blank line after the body.
5. Breaking changes must be indicated either as `!` after the type/scope (e.g. `feat!: `) or via a `BREAKING CHANGE:` footer.
6. Types are not case-sensitive (except `BREAKING CHANGE`).

## Examples

### Simple
```
docs: correct spelling of CHANGELOG
```

### With scope
```
feat(lang): add Polish language
```

### With body and breaking change footer
```
feat: allow provided config object to extend other configs

BREAKING CHANGE: `extends` key in config file is now used for extending other config files
```

### With `!` prefix (breaking change)
```
feat!: send an email to the customer when a product is shipped
```

### Multi-paragraph body with footers
```
fix: prevent racing of requests

Introduce a request id and a reference to latest request. Dismiss
incoming responses other than from latest request.

Remove timeouts which were used to mitigate the racing issue but are
obsolete now.

Reviewed-by: Z
Refs: #123
```

## Frequently Asked Questions

**Q: Should the type be lowercase?**
Any casing works, but stay consistent (lowercase is conventional).

**Q: What if a commit matches more than one type?**
Squash into separate commits. Part of the point is to encourage organized commits.

**Q: How does this relate to SemVer?**
`fix` → PATCH, `feat` → MINOR, `BREAKING CHANGE` → MAJOR.

**Q: How to handle the initial development phase?**
Proceed as if already released. Teammates and tools are the audience.

**Q: How to write revert commits?**
```
revert: let us never again speak of the noodle incident

Refs: 676104e, a215868
```

**Q: Do all contributors need to use this format?**
No. In a squash-merge workflow, the merge commit can be reformatted by the maintainer.

**Q: How to handle a mistake (wrong type used)?**
Before merge: `git rebase -i` to rewrite. After merge: clean up via the team's release process.
