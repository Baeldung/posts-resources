import os
import pickle
from typing import Optional

import numpy as np
import faiss
from openai import OpenAI
import config as cfg

client = OpenAI(api_key=cfg.OPENAI_API_KEY)
print("Loading vector index and chunks...")
index_path = os.path.join(cfg.VECTOR_DIR, "baeldung.idx")
chunks_path = os.path.join(cfg.VECTOR_DIR, "chunks.pkl")
index = faiss.read_index(index_path)

with open(chunks_path, "rb") as f:
    chunk_data = pickle.load(f)
    all_texts = chunk_data["texts"]
    all_meta = chunk_data["meta"]

print(f"Loaded index with {len(all_texts)} chunks")


def retrieve_context(question, k=3):
    q_emb = client.embeddings.create(
        model=cfg.EMBEDDING_MODEL,
        input=question
    ).data[0].embedding

    D, I = index.search(np.array([q_emb], dtype="float32"), k)
    relevant_chunks = []

    for i in I[0]:
        if i == -1:
            continue
        relevant_chunks.append({"text": all_texts[i], "meta": all_meta[i]})

    return relevant_chunks


def chat_round(user_question: str, history: Optional[list[dict]] = None) -> str:
    """Handle a complete conversation turn with RAG."""
    history = history or []

    # Retrieve relevant knowledge base content
    relevant_chunks = retrieve_context(user_question)

    # Format retrieved content for inclusion in prompt
    context_text = ""
    for chunk in relevant_chunks:
        meta = chunk["meta"]
        source = meta.get("source", "Unknown")
        article_id = meta.get("article_id", "")

        context_text += f"[Source: {source}"
        if article_id:
            context_text += f", Article ID: {article_id}]"
        else:
            context_text += "]"

        context_text += f"\n{chunk['text']}\n\n"
    # Create system message with retrieved context
    system_content = (
        "You are customer-support agent. Answer questions about "
        "Spring, Java, and web development. Use the following articles "
        "as context for your answer.\n\n"
        f"Knowledge Base Context:\n{context_text}\n\n"
        "If the context doesn't contain relevant information, use your general knowledge "
        "but prioritize the context's perspective."
    )

    system_msg = {"role": "system", "content": system_content}

    # Prepare conversation messages
    history_msgs = history.copy()
    history_msgs.insert(0, system_msg)
    history_msgs.append({"role": "user", "content": user_question})

    # Make API call without function definitions
    resp = client.chat.completions.create(
        model=cfg.GPT_MODEL,
        messages=history_msgs,
        temperature=0
    )

    # Return direct response
    return resp.choices[0].message.content