# Contributing to sls

Thank you for your interest in contributing!

## License

By contributing, you agree that your contributions will be licensed under
the same terms as the project: **GPL-2.0-or-later**.

## How to contribute

1. Fork the repository.
2. Create a feature branch (`git checkout -b feature/my-change`).
3. Make your changes.
4. Ensure the program still builds cleanly:
   ```bash
   make clean && make
   ```
5. Test basic usage.
6. Commit with a clear message.
7. Open a pull request.

## Code style

- Follow existing style in `sls.c` (POSIX C, no unnecessary dependencies).
- Keep the scp-like interface stable.
- Update the man page (`sls.1`) if you change the interface.

## Contact

Aki Korhonen <spammagnet@eibrak.com>
