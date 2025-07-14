#!/usr/bin/env python3
"""
CLI tool for interacting with the RAG-powered chatbot.
Provides an interactive question-answering interface.
"""

import os
import sys
import argparse
from typing import List, Dict, Optional
import readline  # For better input handling
from pathlib import Path

# Add the src directory to the path to import our modules
sys.path.append(os.path.join(os.path.dirname(__file__), ''))

try:
    from chat import chat_round, retrieve_context
    import config as cfg
except ImportError as e:
    print(f"Error importing modules: {e}")
    print("Make sure you're running this from the project root directory")
    print("and that the vector index has been created by running ingest.py")
    sys.exit(1)


class ChatBot:
    """Interactive chatbot with conversation history."""

    def __init__(self):
        self.history: List[Dict[str, str]] = []
        self.session_active = True

    def add_to_history(self, role: str, content: str):
        """Add a message to conversation history."""
        self.history.append({"role": role, "content": content})

    def get_history(self) -> List[Dict[str, str]]:
        """Get conversation history (excluding system messages)."""
        return [msg for msg in self.history if msg["role"] != "system"]

    def clear_history(self):
        """Clear conversation history."""
        self.history.clear()
        print("✓ Conversation history cleared")

    def show_sources(self, question: str, k: int = 3):
        """Show the sources that would be retrieved for a question."""
        print(f"\n📚 Top {k} sources for: '{question}'")
        print("-" * 50)

        try:
            chunks = retrieve_context(question, k=k)
            for i, chunk in enumerate(chunks, 1):
                meta = chunk["meta"]
                source = meta.get("source", "Unknown")
                article_id = meta.get("article_id", "")

                print(f"{i}. Source: {source}")
                if article_id:
                    print(f"   Article ID: {article_id}")

                # Show first 200 characters of the chunk
                text_preview = chunk["text"][:200]
                if len(chunk["text"]) > 200:
                    text_preview += "..."
                print(f"   Preview: {text_preview}")
                print()

        except Exception as e:
            print(f"Error retrieving sources: {e}")

    def ask_question(self, question: str) -> str:
        """Ask a question and get a response."""
        try:
            # Get response using the chat_round function
            response = chat_round(question, self.get_history())

            # Add to history
            self.add_to_history("user", question)
            self.add_to_history("assistant", response)

            return response

        except Exception as e:
            return f"Error processing question: {e}"

    def interactive_mode(self):
        """Run interactive question-answering session."""
        print("🤖 RAG-Powered Support Chatbot")
        print("=" * 50)
        print("Ask questions about Spring, Java, and web development!")
        print("\nCommands:")
        print("  /help     - Show this help message")
        print("  /sources  - Show sources for your next question")
        print("  /history  - Show conversation history")
        print("  /clear    - Clear conversation history")
        print("  /quit     - Exit the chatbot")
        print("=" * 50)

        while self.session_active:
            try:
                # Get user input
                user_input = input("\n💬 You: ").strip()

                if not user_input:
                    continue

                # Handle commands
                if user_input.startswith('/'):
                    self.handle_command(user_input)
                    continue

                # Process regular question
                print("\n🤔 Thinking...")
                response = self.ask_question(user_input)
                print(f"\n🤖 Assistant: {response}")

            except KeyboardInterrupt:
                print("\n\n👋 Goodbye!")
                break
            except EOFError:
                print("\n\n👋 Goodbye!")
                break

    def handle_command(self, command: str):
        """Handle special commands."""
        cmd = command.lower().strip()

        if cmd == '/help':
            print("\n📖 Available Commands:")
            print("  /help     - Show this help message")
            print("  /sources  - Show sources for your next question")
            print("  /history  - Show conversation history")
            print("  /clear    - Clear conversation history")
            print("  /quit     - Exit the chatbot")

        elif cmd == '/sources':
            question = input("Enter question to show sources for: ").strip()
            if question:
                self.show_sources(question)

        elif cmd == '/history':
            self.show_history()

        elif cmd == '/clear':
            self.clear_history()

        elif cmd == '/quit':
            print("\n👋 Goodbye!")
            self.session_active = False

        else:
            print(f"Unknown command: {command}")
            print("Type /help for available commands")

    def show_history(self):
        """Display conversation history."""
        if not self.history:
            print("\n📝 No conversation history yet")
            return

        print("\n📝 Conversation History:")
        print("-" * 30)

        for i, msg in enumerate(self.history, 1):
            role = "You" if msg["role"] == "user" else "Assistant"
            content = msg["content"]

            # Truncate long messages
            if len(content) > 100:
                content = content[:100] + "..."

            print(f"{i}. {role}: {content}")


def single_question_mode(question: str, show_sources: bool = False):
    """Handle a single question without interactive mode."""
    if show_sources:
        print("📚 Retrieved Sources:")
        print("-" * 30)
        try:
            chunks = retrieve_context(question, k=3)
            for i, chunk in enumerate(chunks, 1):
                meta = chunk["meta"]
                source = meta.get("source", "Unknown")
                article_id = meta.get("article_id", "")

                print(f"{i}. {source}")
                if article_id:
                    print(f"   Article ID: {article_id}")
                print()
        except Exception as e:
            print(f"Error retrieving sources: {e}")
        print("-" * 30)

    try:
        response = chat_round(question)
        print(f"\n🤖 Answer: {response}")
    except Exception as e:
        print(f"Error: {e}")


def main():
    """Main CLI entry point."""
    parser = argparse.ArgumentParser(
        description="Interactive CLI for RAG-powered chatbot",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python cli.py                           # Interactive mode
  python cli.py -q "How do I use Spring?" # Single question
  python cli.py -q "Java concurrency" -s  # Show sources too
        """
    )

    parser.add_argument(
        "-q", "--question",
        help="Ask a single question (non-interactive mode)"
    )

    parser.add_argument(
        "-s", "--sources",
        action="store_true",
        help="Show retrieved sources (works with -q)"
    )

    parser.add_argument(
        "-k", "--top-k",
        type=int,
        default=3,
        help="Number of sources to retrieve (default: 3)"
    )

    args = parser.parse_args()

    # Check if vector store exists
    vector_dir = Path(cfg.VECTOR_DIR)
    if not vector_dir.exists() or not (vector_dir / "baeldung.idx").exists():
        print("❌ Vector store not found!")
        print("Please run ingest.py first to create the vector index.")
        sys.exit(1)

    # Single question mode
    if args.question:
        single_question_mode(args.question, args.sources)
        return

    chatbot = ChatBot()
    chatbot.interactive_mode()


if __name__ == "__main__":
    main()