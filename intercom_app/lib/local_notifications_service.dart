import 'package:flutter_local_notifications/flutter_local_notifications.dart';
import 'package:flutter/material.dart';

class LocalNotificationsService {
  final FlutterLocalNotificationsPlugin _flutterLocalNotificationsPlugin =
      FlutterLocalNotificationsPlugin();

  // Constructor
  LocalNotificationsService();

  Future<void> initialize(BuildContext context) async {
    const AndroidInitializationSettings androidInitializationSettings =
        AndroidInitializationSettings('@mipmap/ic_launcher');

    final DarwinInitializationSettings iOSInitializationSettings =
        DarwinInitializationSettings();

    final InitializationSettings initializationSettings =
        InitializationSettings(
      android: androidInitializationSettings,
      iOS: iOSInitializationSettings,
    );

    await _flutterLocalNotificationsPlugin.initialize(
      initializationSettings,
      onDidReceiveNotificationResponse: (NotificationResponse response) async {
        _handleNotificationClick(context, response.payload);
      },
    );
  }

  void _handleNotificationClick(BuildContext context, String? payload) {
    if (payload != null) {
      print('Notification payload: $payload');
      // Navigate to the desired page, e.g., MessageDetailPage
      Navigator.of(context).pushNamed('/messageDetail', arguments: payload);
    } else {
      print('Notification clicked with no payload');
    }
  }

  Future<void> showNotification({
    int id = 0,
    required String title,
    required String body,
    String? payload, // Add payload parameter
  }) async {
    const AndroidNotificationDetails androidPlatformChannelSpecifics =
        AndroidNotificationDetails(
      '2', // Channel ID
      'Smart Intercom Notifications', // Channel name
      channelDescription: 'This channel is used for Smart Intercom notifications',
      importance: Importance.max,
      priority: Priority.high,
      playSound: true,
      sound: RawResourceAndroidNotificationSound('doorbell'),
    );

    const NotificationDetails platformChannelSpecifics =
        NotificationDetails(android: androidPlatformChannelSpecifics);

    await _flutterLocalNotificationsPlugin.show(
      id,
      title,
      body,
      platformChannelSpecifics,
      payload: payload, // Pass the payload here
    );
  }

  Future<void> showBigTextNotification({
    int id = 0,
    required String title,
    required String body,
    String? payload, // Add payload parameter
  }) async {
    const AndroidNotificationDetails androidPlatformChannelSpecifics =
        AndroidNotificationDetails(
      '2', // Channel ID
      'Smart Intercom Notifications', // Channel name
      channelDescription: 'This channel is used for Smart Intercom notifications',
      importance: Importance.max,
      priority: Priority.high,
      styleInformation: BigTextStyleInformation(''),
      playSound: true,
      sound: RawResourceAndroidNotificationSound('doorbell'),
    );

    const NotificationDetails platformChannelSpecifics =
        NotificationDetails(android: androidPlatformChannelSpecifics);

    await _flutterLocalNotificationsPlugin.show(
      id,
      title,
      body,
      platformChannelSpecifics,
      payload: payload, // Pass the payload here
    );
  }
}
