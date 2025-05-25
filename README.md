# Console-Based ChatApp using C and File Handling

This is a simple console-based chat application written in C that uses file handling to store user data and messages.

---

## 🧩 Features

- 🔐 **User Registration and Login**
- 💬 **Text-based Chat Interface**
- 📁 **File-based Message Storage**
- 📜 **Offline Messaging Support**
- 🧵 **Simple Console UI**
- 🧑‍💻 **Built using Standard C (GCC compatible)**

---

## 🛠 Requirements

- GCC or any C compiler
- Terminal/Console
- Basic knowledge of C and file handling

---

## 🚀 Getting Started

### 1. Clone the Repository
```bash
git clone https://github.com/yourusername/console-chatapp-c.git
cd console-chatapp-c
For Sever:
gcc -o server.exe server.c -lws2_32
.\server.exe
For Client:
gcc -o client.exe client.c -lws2_32
.\client.exe [Your IP Address] 8888
```

##📦 console-chatapp-c
├── Server.c              # Main driver code
├── Client.c              # User registration and login logic
├── message.c           # Message handling (send/receive)
├── user.h              # Header file for user-related functions
├── message.h           # Header file for message functions
├── data/               # Folder to store user and chat files
└── README.md           # This file

##🤝 Contributing
Fork the repository.

Create your feature branch: git checkout -b feature-name

Commit your changes: git commit -m "Add feature"

Push to the branch: git push origin feature-name

Open a pull request.

##📄 License
This project is licensed under the MIT License. See the LICENSE file for details.


##📧 Contact

Let me know if you'd like this personalized with your GitHub username, repo link, or if you'd like a badge section (like build status, license, etc.) added.
