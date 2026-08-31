# GitHub language recognition

GitHub cannot display a Kyna percentage or the official purple `#6D4AFF` language bar until GitHub Linguist recognizes `.kyna`. An unknown language name in `.gitattributes` does not create support and must not be used to misclassify Kyna as another language.

The repository maintains the MIT-licensed TextMate grammar at `editors/vscode-kyna/syntaxes/kyna.tmLanguage.json` with scope `source.kyna`, plus representative non-tutorial samples. Before an upstream submission, public adoption must satisfy Linguist’s then-current usage requirements—currently approximately 2,000 indexed files from a varied set of public repositories.

When that evidence exists, the upstream entry should declare:

- name: `Kyna`
- type: `programming`
- extension: `.kyna`
- TextMate scope: `source.kyna`
- color: `#6D4AFF`
- a generated language ID, licensed grammar, representative samples, and public-usage evidence

Only after support ships on GitHub should this repository add a confirming `.gitattributes` rule and validate it with `github-linguist --breakdown --strategies`. GitHub recalculates its language bar asynchronously after default-branch updates.
