"""
Baeldung Article CS-

Multimodal search using LLAMAINDEX
"""
import PyPDF2
import faiss
from llama_index.core import VectorStoreIndex
from llama_index.core import Document
from llama_index.llms.huggingface.base import HuggingFaceLLM
import cv2
from llama_index.embeddings.huggingface import HuggingFaceEmbedding
import whisper
import numpy as np
import glob
import os
from PIL import Image
import os
from transformers import BlipProcessor, BlipForConditionalGeneration
from sentence_transformers import SentenceTransformer
from llama_index.core import Settings

import warnings
warnings.filterwarnings("ignore", category=UserWarning, module='av')
warnings.filterwarnings("ignore", category=UserWarning, module='decord')


os.environ["TOKENIZERS_PARALLELISM"] = "false"

# Load SentenceTransformer model for generating embeddings
embedding_model = SentenceTransformer('all-MiniLM-L6-v2')
embedding_model = embedding_model.to('cpu')

print('Loading BLIP Model')
# Initialize open-source BLIP for image and video feature extraction
blip_processor = BlipProcessor.from_pretrained("Salesforce/blip-image-captioning-base")
blip_model = BlipForConditionalGeneration.from_pretrained("Salesforce/blip-image-captioning-base")
print('BLIP Model loaded successfully')

# Initialize Whisper for audio transcription
print('Loading Whisper Model')
audio_model = whisper.load_model("base")
print('Whisper Model loaded successfully')


def process_pdf(pdf_path):
    """Convert text into embeddings using Sentence-BERT."""
    with open(pdf_path, 'rb') as file:
        reader = PyPDF2.PdfReader(file)
        text = ''
        for page_number in range(len(reader.pages)):
            text += reader.pages[page_number].extract_text()

    return text


def process_image(image_path):
    """Convert image to CLIP embeddings."""
    """Use BLIP to generate a caption for the image."""
    try:
        # Check if image is a string path or ndarray
        if isinstance(image_path, str):
            # If it's a path, open the image
            image = Image.open(image_path)
        elif isinstance(image_path, np.ndarray):
            # If it's an ndarray, convert it to a PIL Image
            image = Image.fromarray(cv2.cvtColor(image_path, cv2.COLOR_BGR2RGB))

        inputs = blip_processor(image, return_tensors="pt")
        out = blip_model.generate(**inputs, max_new_tokens=256)
        caption = blip_processor.decode(out[0], skip_special_tokens=True)
        return caption
    except Exception as e:
        print(f"Error processing image {image_path}: {e}")
        return None


def process_audio(audio_path):
    """Transcribe audio using Whisper and convert the text to embeddings."""
    result = audio_model.transcribe(audio_path, fp16=False)
    text = result['text']
    print(f"Audio Transcript: {text}")
    return text


def process_video(video_path):
    """Extract text from a video using the BLIP model on frames."""
    captions = []
    try:
        cap = cv2.VideoCapture(video_path)
        frame_count = 0
        while cap.isOpened():
            ret, frame = cap.read()
            if not ret:
                break
            if frame_count % 30 == 0:  # Process one frame every second (adjust as needed)
                caption = process_image(frame)
                if caption:
                    captions.append(caption)
            frame_count += 1
        cap.release()
        return " ".join(captions)
    except Exception as e:
        print(f"Error processing video {video_path}: {e}")
        return None


# Store metadata for each document, image, audio, and video
document_store = []


def add_pdf(pdf_path):
    """Add a PDF file to the FAISS index after extracting text."""
    text = process_pdf(pdf_path)
    document_store.append({'type': 'pdf', 'content': text, 'path': pdf_path})


def add_image(image_path):
    """Add images to the FAISS index."""
    description = process_image(image_path)
    document_store.append({'type': 'image', 'content': description, 'path': image_path})


def add_audio(audio_path):
    """Add audio files to the FAISS index."""
    text = process_audio(audio_path)
    document_store.append({'type': 'audio', 'content': text, 'path': audio_path})


def add_video(video_path):
    """Add video files to the FAISS index."""
    description = process_video(video_path)
    document_store.append({'type': 'video', 'content': description, 'path': video_path})


print('Converting all modalities to text and adding to FAISS')

source_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'source')

# Adding multimodal content
pdf_path = os.path.join(source_path, 'pdf')
pdf_files = glob.glob(pdf_path + '/' + "*.pdf")

for file in pdf_files:
    add_pdf(file)
print('Pdf Data added to FAISS')

audio_path = os.path.join(source_path, 'audio')
audio_files = glob.glob(audio_path + '/' + "*.wav")
for file in audio_files:
    add_audio(file)

print('Audio Data added to FAISS')

image_path = os.path.join(source_path, 'image')
image_files = glob.glob(image_path + '/' + "*.jpg")
for file in image_files:
    add_image(file)

print('Image Data added to FAISS')

video_path = os.path.join(source_path, 'video')
video_files = glob.glob(video_path + '/' + "*.mp4")
for file in video_files:
    add_video(file)

print('All data items added to FAISS')


def create_faiss_index():
    """Create a FAISS index for fast similarity search."""
    texts = [item['content'] for item in document_store if item['content'] is not None]

    # Generate embeddings for each text using SentenceTransformer
    embeddings = embedding_model.encode(texts, device='cpu', convert_to_tensor=True)
    embeddings = embeddings.cpu().numpy().astype('float32')

    # Create FAISS index
    index = faiss.IndexFlatL2(embeddings.shape[1])  # L2 distance
    index.add(embeddings)  # Add embeddings to the index

    files = [item['path'] for item in document_store if item['content'] is not None]
    return index, texts, files


def search_faiss_index(query, index, texts, files):
    """Search the FAISS index for the closest match to the query."""
    # Generate embedding for the query using SentenceTransformer
    query_embedding = embedding_model.encode(query, convert_to_tensor=True).numpy().astype('float32')

    # Ensure query_embedding is 2D
    query_embedding = query_embedding.reshape(1, -1)  # Reshape to (1, embedding_dimension)
    D, I = index.search(query_embedding, k=3)  # Get top 3 results

    # Debug: Check if indices are being retrieved correctly
    print(f"FAISS indices: {I}")
    print(f"FAISS distances: {D}")

    results = [{'file_name': files[i], 'relevant_chunk': texts[i], 'distance': D[0][j]} for j, i in enumerate(I[0])]
    return results


def multimodal_search(query):
    """Perform multimodal search across all documents."""
    results = []
    index, texts, files = create_faiss_index()  # Create the FAISS index

    # Search the index
    faiss_results = search_faiss_index(query, index, texts, files)

    for result in faiss_results:
        results.append(result)

    return results


def integrate_llamaindex(query):
    """Use LLaMAindex to fetch more factual data."""
    small_model_name = "gpt2"  # You can also try "distilbert-base-uncased" for smaller tasks

    # Initialize the tokenizer and model without quantization
    from transformers import AutoModelForCausalLM, AutoTokenizer
    tokenizer = AutoTokenizer.from_pretrained(small_model_name)
    llm_model = AutoModelForCausalLM.from_pretrained(small_model_name)

    # Configure HuggingFaceLLM with the smaller model
    llm = HuggingFaceLLM(
        context_window=512,  # Adjusted context window for smaller models
        max_new_tokens=128,  # Reduce token generation for smaller model
        generate_kwargs={"temperature": 0.95, "top_k": 50, "top_p": 0.95, "do_sample": False},
        tokenizer=tokenizer,
        model=llm_model,
        tokenizer_name=small_model_name,
        model_name=small_model_name,
        device_map="auto",

        # Ensure it runs on CPU for MacBook compatibility
        tokenizer_kwargs={"max_length": 512, 'pad_token_id': tokenizer.eos_token_id, "eos_token_id": [128001, 128009]},
    )
    Settings.chunk_size = 512
    Settings.llm = llm

    Settings.embed_model = HuggingFaceEmbedding(
        model_name="BAAI/bge-small-en-v1.5"
    )

    # Read documents for the index (ensure proper extraction from document_store)
    docs = []
    for item in document_store:
        if isinstance(item['content'], str):  # Check if content is a valid string
            docs.append(Document(text=item['content']))
        else:
            print(f"Warning: Skipping item with non-string content: {item}")

    # Debug: Check the number of loaded documents
    print(f"Number of documents loaded: {len(docs)}")

    # Create the index with the local Hugging Face LLM
    if docs:

        # Create the index with the local Hugging Face LLM
        try:
            index = VectorStoreIndex.from_documents(docs, show_progress=False)

            # Debug: Inspect indexed documents
            print("Indexed documents:")
            for doc in docs:
                print(f"- {doc.text[:10]}...")  # Print the first 100 characters of each document for inspection

        except Exception as e:
            print(f"Error creating index: {e}")
            return []

        query_engine = index.as_query_engine(streaming=False, verbose=True)
        response = query_engine.query(query)
        return response

    else:
        print("No documents to index.")
        return []


# Issue a search query
query = input("What is a cat?")

# Plain FAISS based usage:
results = multimodal_search(query)

# Display results
for result in results:
    print(
        f"Matched in file: {result['file_name']} on text: {result['relevant_chunk']} with Distance: {result['distance']}")

# # LLaMAindex based integration for a coherent, factual response
llama_response = integrate_llamaindex(query)
print(f"LlamaIndex Response as per its Own Konwledge: \n\n{str(llama_response)}\n")

print(f"\nLlamaIndex Response as per Our Data sources:\n")
for node_with_score in llama_response.source_nodes:
    print(node_with_score.text)
    print(node_with_score.score)
