import os
from pathlib import Path
import pandas as pd, numpy as np, pickle, faiss
from openai import OpenAI
from langchain.text_splitter import RecursiveCharacterTextSplitter

import config as cfg
from src.pdf_utils import pdf_to_text

client = OpenAI(api_key=cfg.OPENAI_API_KEY)

# -------------------------------------------------------------------------
# 1  Load Baeldung article blurbs from CSV → narrative snippets
# -------------------------------------------------------------------------


raw_chunks: list[str] = []
metas:       list[dict] = []

art_df = pd.read_csv(cfg.ARTICLES_CSV)
for _, row in art_df.iterrows():
    payload = (
        f"Article ID: {row['article_id']}\n"
        f"Title: {row['title']}\n"
        f"Category: {row['category']}\n"
        f"Reading time: {row['reading_time_minutes']} min\n"
        f"Summary: {row['summary']}"
    )
    raw_chunks.append(payload)
    metas.append({"source": "articles.csv", "article_id": row['article_id']})

# -------------------------------------------------------------------------
# 2  Load supplementary PDFs (white-papers, tutorials, etc.)
# -------------------------------------------------------------------------
for pdf_path in Path(cfg.PDF_DIR).glob("*.pdf"):
    text = pdf_to_text(pdf_path)
    raw_chunks.append(text)
    metas.append({"source": pdf_path.name})

print(f"Ingested {len(raw_chunks)} base documents")

# -------------------------------------------------------------------------
# 3  Chunk splitting (LangChain RecursiveCharacter splitter)
# -------------------------------------------------------------------------
splitter = RecursiveCharacterTextSplitter(
    chunk_size   = cfg.CHUNK_SIZE,
    chunk_overlap= cfg.CHUNK_OVERLAP
)

chunks, chunk_meta = [], []
for txt, meta in zip(raw_chunks, metas):
    pieces = splitter.split_text(txt)
    chunks.extend(pieces)
    chunk_meta.extend([meta] * len(pieces))

print(f"Generated {len(chunks)} chunks")

# -------------------------------------------------------------------------
# 4  Embeddings (OpenAI v3-small)
# -------------------------------------------------------------------------
embeds = []
for piece in chunks:
    emb_resp = client.embeddings.create(model=cfg.EMBEDDING_MODEL, input=piece)
    embeds.append(emb_resp.data[0].embedding)

embeds_np = np.array(embeds, dtype="float32")

# -------------------------------------------------------------------------
# 5  FAISS index + persistence
# -------------------------------------------------------------------------
index = faiss.IndexFlatL2(len(embeds_np[0]))
index.add(embeds_np)

Path(cfg.VECTOR_DIR).mkdir(parents=True, exist_ok=True)
faiss.write_index(index, os.path.join(cfg.VECTOR_DIR, "baeldung.idx"))
with open(os.path.join(cfg.VECTOR_DIR, "chunks.pkl"), "wb") as f:
    pickle.dump({"texts": chunks, "meta": chunk_meta}, f)

print("✓ Vector index persisted →", cfg.VECTOR_DIR)