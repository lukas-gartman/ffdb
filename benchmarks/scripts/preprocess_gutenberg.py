import sys
import re
from pathlib import Path

def tokenize(text: str):
    text = text.lower()
    tokens = re.findall(r"[a-z]+(?:'[a-z]+)?", text)
    return tokens

def preprocess_file(input_path: Path, output_path: Path):
    if not input_path.is_file():
        print(f"Error: input file does not exist: {input_path}")
        sys.exit(1)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    print(f"Processing {input_path.name}...")
    text = input_path.read_text(encoding="utf-8", errors="ignore")
    tokens = tokenize(text)
    with output_path.open("w", encoding="utf-8") as out_f:
        out_f.write("\n".join(tokens) + "\n")
    print(f"Done. Wrote tokens to: {output_path}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python preprocess_gutenberg.py input_file.txt output_file.txt")
        sys.exit(1)

    input_path = Path(sys.argv[1])
    output_path = Path(sys.argv[2])
    preprocess_file(input_path, output_path)
