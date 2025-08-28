from pathlib import Path
from typing import Union

from PyPDF2 import PdfReader

def pdf_to_text(path: Union[str, Path]) -> str:
    reader = PdfReader(str(path))
    return "\n".join(page.extract_text() or "" for page in reader.pages)