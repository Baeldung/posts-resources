import os
import openai

OPENAI_API_KEY = "sk-..."
openai.api_key = OPENAI_API_KEY
GPT_MODEL = "gpt-4-0613"
EMBEDDING_MODEL = "text-embedding-ada-002"
VECTOR_DIR = "../vector_store"
CHUNK_SIZE = 1000
CHUNK_OVERLAP = 100

ARTICLES_CSV = "docs/articles.csv"
PDF_DIR = "../docs/"

