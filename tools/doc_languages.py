"""Writes the language line at the top of every translated document.

usage: python tools/doc_languages.py

For each document (README.md, HANDOFF.md, ...) the translations are X.<lang>.md next to it.
The line under the title links all the languages that exist, the current one as plain text:
    English | [Русский](README.ru.md) | [Українська](README.uk.md) | ...
In a new translation, put the word LANGUAGES where that line goes.
"""
import os
import re

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
DOCUMENTS = ['README.md', 'HANDOFF.md', 'docs/HOW_IT_WORKS.md', 'docs/NOTES.md', 'port/README.md',
	'oracle/README.md', 'rom/README.md']
LANGUAGES = [('en', 'English'), ('ru', 'Русский'), ('uk', 'Українська'), ('de', 'Deutsch'),
	('pl', 'Polski'), ('it', 'Italiano'), ('es', 'Español'), ('zh', '中文'), ('ja', '日本語')]


def file_for(document, lang):
	return document if lang == 'en' else document[:-3] + f'.{lang}.md'


def is_language_line(line):
	return line.strip() == 'LANGUAGES' or re.match(r'^\[?English\]?(\(|\s*\|)', line.strip()) is not None


def main():
	for document in DOCUMENTS:
		present = [(lang, name) for lang, name in LANGUAGES
			if os.path.exists(os.path.join(ROOT, file_for(document, lang)))]
		for lang, _ in present:
			path = os.path.join(ROOT, file_for(document, lang))
			parts = []
			for other, name in present:
				target = os.path.basename(file_for(document, other))
				parts.append(name if other == lang else f'[{name}]({target})')
			line = ' | '.join(parts)
			text = open(path, encoding='utf-8').read().split('\n')
			for i, old in enumerate(text[:6]):
				if is_language_line(old):
					text[i] = line
					break
			else:
				raise SystemExit(f'{path}: no language line near the top')
			open(path, 'w', encoding='utf-8').write('\n'.join(text))


if __name__ == '__main__':
	main()
