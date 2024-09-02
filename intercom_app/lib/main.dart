import 'dart:async';
import 'dart:convert';

import 'package:audioplayers/audioplayers.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import "local_notifications_service.dart";
import 'package:permission_handler/permission_handler.dart';


void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Firebase.initializeApp(
    options: FirebaseOptions(
      apiKey: "AIzaSyASO4iwGRr6o4BvFRgIIzfAISH7Is-TAPA",
      authDomain: "smartintercom-b517f.firebaseapp.com",
      databaseURL: "https://smartintercom-b517f-default-rtdb.europe-west1.firebasedatabase.app",
      projectId: "smartintercom-b517f",
      storageBucket: "smartintercom-b517f.appspot.com",
      messagingSenderId: "964303376551",
      appId: "1:964303376551:web:d6b7b28ad344d950314b82",
      measurementId: "", // Add measurementId if you have one
    ),
  );

  // Clear chat history
  await clearChatHistory();

  runApp(MyApp());
}

Future<void> clearChatHistory() async {
  final DatabaseReference messagesRef = FirebaseDatabase.instance.ref().child('/messages');
  await messagesRef.remove();
}

class MyApp extends StatefulWidget {
  @override
  _MyAppState createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  bool _isDarkMode = false;

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'Smart Intercom',
      theme: _isDarkMode ? ThemeData.dark() : ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.lightBlueAccent),
        useMaterial3: true,
        scaffoldBackgroundColor: Colors.lightBlue[50], // Light blue background
        appBarTheme: AppBarTheme(
          backgroundColor: Colors.lightBlueAccent,
          foregroundColor: Colors.white,
        ),
        inputDecorationTheme: InputDecorationTheme(
          filled: true,
          fillColor: Colors.white,
          border: OutlineInputBorder(
            borderRadius: BorderRadius.circular(10.0),
          ),
        ),
      ),
      home: MyHomePage(
        title: 'Smart Intercom',
        isDarkMode: _isDarkMode,
        toggleTheme: _toggleTheme,
      ),
    );
  }

  void _toggleTheme() {
    setState(() {
      _isDarkMode = !_isDarkMode;
    });
  }
}

class MyHomePage extends StatefulWidget {
  const MyHomePage({super.key, required this.title, required this.isDarkMode, required this.toggleTheme});

  final String title;
  final bool isDarkMode;
  final Function toggleTheme;

  @override
  State<MyHomePage> createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> {
  final DatabaseReference _database = FirebaseDatabase.instance.ref();
  final AudioPlayer _audioPlayer = AudioPlayer();
  Uint8List? _mergedAudioBytes;
  List<Map<String, dynamic>> _messages = [];
  final TextEditingController _controller = TextEditingController();
  final TextEditingController _passwordController = TextEditingController();
  final TextEditingController _durationController = TextEditingController();
  bool _isFileMerged = false;
  String? _replyToMessageId;
  bool _isPlaying = false;  // Track whether the audio is playing or paused
  String? _currentAudioId;  // Track the ID of the currently playing audio
  Set<String> _repliedMessages = {}; // Track which messages have been replied to

  DateTime? _lastUploadStatusChange;
  Timer? _uploadStatusTimer;
  int count = 0; // Make count a class variable

  final LocalNotificationsService _localNotificationsService = LocalNotificationsService();


  @override
  void initState() {
    super.initState();
    _requestNotificationPermission(); // Request notification permission
    _fetchLastWAVString();
    _listenForMessages();
    _listenForWAVStringChanges();
    _listenForAudioChunks();

    // Initialize the local notification service
    _localNotificationsService.initialize(context);

    _audioPlayer.onPlayerStateChanged.listen((state) {
      setState(() {
        _isPlaying = state == PlayerState.playing;
      });
    });
  }
  void _requestNotificationPermission() async {
    final status = await Permission.notification.request();
    if (status.isGranted) {
      print('Notification permission granted');
    } else if (status.isDenied) {
      print('Notification permission denied');
    } else if (status.isPermanentlyDenied) {
      print('Notification permission permanently denied');
      // Consider taking the user to the app settings to enable the permission
    }
  }

  Future<void> _fetchLastWAVString() async {
    final DatabaseReference ref = _database.child('/ICST/WAVString');
    final DataSnapshot snapshot = await ref.get();

    if (snapshot.exists) {
      List<int> audioBytes = [];

      final chunks = Map<String, dynamic>.from(snapshot.value as Map);

      // Extract numerical part from keys, convert to int, and sort
      final sortedKeys = chunks.keys
          .map((key) => int.parse(key.replaceAll(RegExp(r'\D'), '')))
          .toList()
        ..sort();

      // Combine the chunks into a single audio byte array
      for (var key in sortedKeys) {
        final value = chunks['chunk$key'];
        final decodedBytes = base64Decode(value as String);
        audioBytes.addAll(decodedBytes);
      }

      _mergedAudioBytes = Uint8List.fromList(audioBytes);

      setState(() {
        _isFileMerged = true;
      });

      print('Last WAV string fetched and merged');
    } else {
      print('No WAV string found');
    }
  }

  Future<void> _fetchLastMessage() async {
    final Query query = _database.child('/messages').limitToLast(1);  // Get the last message
    final DataSnapshot snapshot = await query.get();

    if (snapshot.exists) {
      final data = Map<String, dynamic>.from((snapshot.value as Map).values.first as Map);
      data['id'] = (snapshot.value as Map).keys.first; // Add the message ID

      setState(() {
        _messages.add(data);
      });

      if (data.containsKey('audioMessage')) {
        print('Last audio message at ${data['audioMessage']['timestamp']}');
        // Optionally, you could auto-play the last audio message or notify the user
      } else if (data.containsKey('message')) {
        print('Last text message: ${data['message']}');
      }
    } else {
      print('No last message found.');
    }
  }

  Future<void> downloadAndMergeChunks() async {
    print('Starting to read chunks...');
    final DatabaseReference ref = _database.child('/ICST/WAVString');
    final snapshot = await ref.get();
    List<int> audioBytes = [];

    if (snapshot.exists) {
      final chunks = Map<String, dynamic>.from(snapshot.value as Map);

      // Extract numerical part from keys, convert to int, and sort
      final sortedKeys = chunks.keys
          .map((key) => int.parse(key.replaceAll(RegExp(r'\D'), '')))
          .toList()
        ..sort();

      // Debug: Print sorted keys
      print('Sorted keys: $sortedKeys');

      for (var key in sortedKeys) {
        final value = chunks['chunk$key'];
        // Debug: Print each chunk before decoding
        print('Processing chunk key: chunk$key');
        final decodedBytes = base64Decode(value as String);
        audioBytes.addAll(decodedBytes);
      }

      _mergedAudioBytes = Uint8List.fromList(audioBytes);

      setState(() {
        _isFileMerged = true;
      });

      print('Audio bytes merged in memory');
    } else {
      print('No data available.');
    }
  }

  void _sendAudioMessage() {
    if (_mergedAudioBytes != null) {
      print('Sending audio message with merged bytes');
      _database.child('/messages').push().set({
        'audioMessage': {
          'data': base64Encode(_mergedAudioBytes!),
          'timestamp': DateTime.now().toString(),
        },
      });
      print('Audio message sent.');
    } else {
      print('No merged audio to send.');
    }
  }
  
  void _playAudioMessage(Uint8List audioBytes, String messageId) async {
    if (_currentAudioId == messageId && _isPlaying) {
      await _audioPlayer.pause();
    } else {
      if (_currentAudioId != messageId) {
        await _audioPlayer.stop(); // Stop any previously playing audio
        _currentAudioId = messageId;
      }
      _audioPlayer.setSourceBytes(audioBytes);
      _audioPlayer.resume(); // Start playing the audio
    }
  }

  Future<void> _sendMessage(String message, {bool isDeny = false, String? messageIdToDelete}) async {
    final DatabaseReference newMessageRef = _database.child('/messages'); // Use push() to generate a new unique ID
    
    // Construct the message to send
    Map<String, dynamic> messageData = {
      'message': isDeny ? 'Visitor denied' : message,
      'replyTo': _replyToMessageId,
      'isDeny': isDeny,
    };

    if (!isDeny) {
      // Only add password and duration if not denying
      final password = _passwordController.text;
      final duration = int.tryParse(_durationController.text) ?? 0;
      
      if (password.length == 4 && duration >= 0 && duration <= 60) {
        messageData['password'] = password;
        messageData['duration'] = duration;
      } else {
        print('Invalid password or duration');
        return;
      }
    }

    // Send the message to Firebase
    newMessageRef.set(messageData).then((_) {
      // Close the reply window by resetting _replyToMessageId to null
      setState(() {
        _repliedMessages.add(_replyToMessageId!); // Add the message ID to the replied messages set
        _replyToMessageId = null;
        if (isDeny && messageIdToDelete != null) {
          _messages.removeWhere((message) => message['id'] == messageIdToDelete);
        }
      });

      print('Message sent successfully.');

      // Schedule deletion after 30 seconds
      Future.delayed(Duration(seconds: 30), () {
        newMessageRef.remove().then((_) {
          print('Message deleted after 30 seconds.');
        }).catchError((error) {
          print('Failed to delete message: $error');
        });
      });

    }).catchError((error) {
      print('Failed to send message: $error');
    });

    _controller.clear();
    _passwordController.clear();
    _durationController.clear();
  }

  Future<void> _confirmAndDeny(String messageId) async {
    final result = await showDialog<bool>(
      context: context,
      builder: (BuildContext context) {
        return AlertDialog(
          title: Text("Confirm Deny"),
          content: Text("Are you sure you want to deny this visitor? This will delete the audio message."),
          actions: <Widget>[
            TextButton(
              child: Text("Cancel"),
              onPressed: () {
                Navigator.of(context).pop(false);
              },
            ),
            TextButton(
              child: Text("Confirm"),
              onPressed: () {
                Navigator.of(context).pop(true);
              },
            ),
          ],
        );
      },
    );

    if (result == true) {
      _sendMessage('Visitor denied', isDeny: true, messageIdToDelete: messageId);
    }
  }

  void _deleteWAVString() {
    final DatabaseReference wavStringRef = _database.child('/ICST/WAVString');
    wavStringRef.remove().then((_) {
      print('WAVString deleted successfully.');
      count = 0; // Reset count when WAVString is deleted
    }).catchError((error) {
      print('Failed to delete WAVString: $error');
    });
  }

  void _deleteUploadStatus() {
    final DatabaseReference uploadStatusRef = _database.child('/ICST/UploadStatus');
    uploadStatusRef.remove().then((_) {
      print('UploadStatus deleted successfully.');
      count = 0; // Reset count when UploadStatus is deleted
    }).catchError((error) {
      print('Failed to delete UploadStatus: $error');
    });
  }

  void _startUploadStatusTimer(){
    _uploadStatusTimer?.cancel(); // Cancel any existing timer
    _uploadStatusTimer = Timer(Duration(minutes: 1), () {
      if (_lastUploadStatusChange != null &&
          DateTime.now().difference(_lastUploadStatusChange!).inMinutes >= 1) {
        print('UploadStatus has not changed for 1 minute. Deleting...');
        _deleteUploadStatus();
      }
    });
  }

  void _listenForAudioChunks() {
    final DatabaseReference ref = _database.child('/ICST/UploadStatus');
    ref.onValue.listen((event) {
      try {
        if (event.snapshot.exists) {
          final status = event.snapshot.value as int;
          _lastUploadStatusChange = DateTime.now(); // Update the last change timestamp
          _startUploadStatusTimer(); // Restart the timer

          if (status == 40) {
            print('All audio chunks uploaded. Merging...');
            _sendAudioMessage(); // Send the merged audio as a message
          }
        } else {
          print('No upload status available.');
        }
      } catch (e) {
        print('Error processing upload status: $e');
      }
    });
  }

  void _listenForMessages() {
    final DatabaseReference ref = _database.child('/messages');
    ref.onChildAdded.listen((event) {
      try {
        final data = Map<String, dynamic>.from(event.snapshot.value as Map);
        data['id'] = event.snapshot.key; // Add the message ID
        setState(() {
          _messages.add(data);
        });
        if (data.containsKey('message')) {
          print(data['isDeny'] == true ? 'Visitor denied' : 'Received message: ${data['message']}');
        } else if (data.containsKey('audioMessage')) {
          print('Received audio message at ${data['audioMessage']['timestamp']}');
                  // Show notification for new audio message
          _localNotificationsService.showNotification(
            title: 'New Audio Message',
            body: 'You have received a new audio message.',
            payload: data['id'], // Optionally pass the message ID as payload
          );
        }
      } catch (e) {
        print('Error processing message: $e');
      }
    });
  }

  void _listenForWAVStringChanges() {
    final DatabaseReference ref = _database.child('/ICST/WAVString');
    ref.onValue.listen((event) async {
      try {
        if (event.snapshot.exists) {
          print('WAVString data changed. Merging chunks...');
          await downloadAndMergeChunks();
          count++;
          _updateUploadStatus(count);
          if(count == 41) {
            count = 0;
          }
        } else {
          print('No WAVString data available.');
        }
      } catch (e) {
        print('Error processing WAVString changes: $e');
      }
    });
  }

  Future<void> _updateUploadStatus(int status) async {
    await _database.child('/ICST/UploadStatus').set(status);
  }
  Future<String?> _fetchCurrentPassword() async {
    final DatabaseReference passwordRef = _database.child('/default_password');
    final DataSnapshot snapshot = await passwordRef.get();

    if (snapshot.exists) {
      return snapshot.value as String?;
    } else {
      print('No default password found.');
      return null;
    }
  }
  // New methods for settings, updating default password, and theme toggle with custom dialog

  void _showSettingsDialog() async {
  final currentPassword = await _fetchCurrentPassword();
  final TextEditingController _newPasswordController = TextEditingController();

  showDialog(
    context: context,
    builder: (BuildContext context) {
      return Dialog(
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12.0),
        ),
        elevation: 16,
        child: Container(
          padding: EdgeInsets.all(16.0),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: <Widget>[
              Text(
                'Settings',
                style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
              ),
              SizedBox(height: 16),
              Text(
                'Enter the new default password',
                style: TextStyle(
                  fontSize: 16,
                  color: Colors.black, // You can adjust the color as needed
                ),
              ),
              SizedBox(height: 8), // Add some space between the label and the TextField
              TextField(
                controller: _newPasswordController,
                style: TextStyle(
                  fontSize: 16,
                  fontWeight: FontWeight.w400,
                  color: Colors.grey[600],
                ),
                decoration: InputDecoration(
                  hintText: currentPassword ?? 'No current password',
                  labelStyle: TextStyle(
                    color: Colors.grey,
                    fontWeight: FontWeight.w400,
                  ),
                ),
                keyboardType: TextInputType.number,
                inputFormatters: [
                  FilteringTextInputFormatter.digitsOnly,
                ],
                maxLength: 4,
              ),
              SwitchListTile(
                title: Text(
                  'Dark Mode',
                  style: TextStyle(
                    color: Colors.lightBlueAccent,
                  ),
                ),
                value: widget.isDarkMode,
                onChanged: (value) {
                  widget.toggleTheme();
                  Navigator.of(context).pop(); // Close the dialog after toggling the theme
                },
              ),
              SizedBox(height: 16),
              Row(
                mainAxisAlignment: MainAxisAlignment.end,
                children: <Widget>[
                  TextButton(
                    child: Text('Cancel', style: TextStyle(color: Colors.lightBlue)),
                    onPressed: () => Navigator.of(context).pop(),
                  ),
                  TextButton(
                    child: Text('Save', style: TextStyle(color: Colors.lightBlue)),
                    onPressed: () async {
                      final newPassword = _newPasswordController.text;
                      if (newPassword.length == 4) {
                        await _updateDefaultPassword(newPassword);
                        Navigator.of(context).pop();
                      } else {
                        // Optionally, you could show an error message here
                      }
                    },
                  ),
                ],
              ),
            ],
          ),
        ),
      );
    },
  );
}



  Future<void> _updateDefaultPassword(String newPassword) async {
    final DatabaseReference defaultPasswordRef = _database.child('/default_password');
    await defaultPasswordRef.set(newPassword).then((_) {
      print('Default password updated successfully.');
    }).catchError((error) {
      print('Failed to update default password: $error');
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.title),
        actions: [
          Padding(
            padding: const EdgeInsets.only(right: 22.0),
            child: IconButton(
              icon: Icon(Icons.settings),
              onPressed: _showSettingsDialog,
            ),
          ),
        ],
      ),
      body: Column(
        children: <Widget>[
          if (_mergedAudioBytes != null) ...[
            ListTile(
              tileColor: Colors.white,
              title: Text(
                'Last Audio Message',
                style: TextStyle(color: Colors.black),
              ),
              trailing: IconButton(
                icon: Icon(
                  _isPlaying && _currentAudioId == 'lastMessage'
                      ? Icons.pause
                      : Icons.play_arrow,
                  color: Colors.lightBlue,
                ),
                onPressed: () {
                  _playAudioMessage(_mergedAudioBytes!, 'lastMessage');
                },
              ),
            ),
            Divider(), // Add a divider between the last audio message and the messages list
          ],
          if (_messages.isEmpty) 
            Expanded(
              child: Center(
                child: Text(
                  //'No messages yet',
                   '',
                  style: TextStyle(color: Colors.black54, fontSize: 18),
                ),
              ),
            )
          else 
            Expanded(
              child: ListView.builder(
                itemCount: _messages.length,
                itemBuilder: (context, index) {
                  final message = _messages[index];
                  return ListTile(
                    tileColor: Colors.white,
                    title: Text(
                      message['message'] ?? 'Audio message at ${message['audioMessage']['timestamp']}',
                      style: TextStyle(color: Colors.black),
                    ),
                    subtitle: message['replyTo'] != null
                        ? Text(
                            'Replying to: ${_messages.firstWhere((msg) => msg['id'] == message['replyTo'], orElse: () => {'message': 'Unknown'})['message']}',
                            style: TextStyle(color: Colors.black54),
                          )
                        : null,
                    trailing: message.containsKey('audioMessage')
                        ? IconButton(
                            icon: Icon(
                              _isPlaying && _currentAudioId == message['id']
                                  ? Icons.pause
                                  : Icons.play_arrow,
                              color: Colors.lightBlue,
                            ),
                            onPressed: () {
                              final audioBytes = base64Decode(message['audioMessage']['data']);
                              _playAudioMessage(audioBytes, message['id']);
                            },
                          )
                        : null,
                    onTap: () {
                      if (!_repliedMessages.contains(message['id'])) { // Check if the message has already been replied to
                        setState(() {
                          _replyToMessageId = message['id'];
                        });
                      }
                    },
                  );
                },
              ),
            ),
          if (_replyToMessageId != null) ...[
            Padding(
              padding: const EdgeInsets.all(8.0),
              child: Column(
                children: [
                  Text(
                    'Replying to: ${_messages.firstWhere((msg) => msg['id'] == _replyToMessageId, orElse: () => {'message': 'Unknown'})['message']}',
                    style: TextStyle(color: Colors.black),
                  ),
                  SizedBox(height: 8),
                  TextField(
                    controller: _passwordController,
                    decoration: InputDecoration(
                      labelText: 'Enter 4-digit password',
                    ),
                    keyboardType: TextInputType.number,
                    maxLength: 4,
                    inputFormatters: [
                      FilteringTextInputFormatter.digitsOnly,
                    ],
                  ),
                  SizedBox(height: 8),
                  TextField(
                    controller: _durationController,
                    decoration: InputDecoration(
                      labelText: 'Enter duration (0-60 seconds)',
                    ),
                    keyboardType: TextInputType.number,
                    inputFormatters: [
                      FilteringTextInputFormatter.digitsOnly,
                    ],
                  ),
                  SizedBox(height: 8),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceEvenly,
                    children: [
                      IconButton(
                        icon: Icon(Icons.send, color: Colors.lightBlue),
                        onPressed: () => _sendMessage(_controller.text),
                      ),
                      if (_messages.firstWhere((msg) => msg['id'] == _replyToMessageId)['audioMessage'] != null)
                        IconButton(
                          icon: Icon(Icons.block, color: Colors.red),
                          onPressed: () => _confirmAndDeny(_replyToMessageId!),
                        ),
                    ],
                  ),
                ],
              ),
            ),
          ],
        ],
      ),
    );
  }
}