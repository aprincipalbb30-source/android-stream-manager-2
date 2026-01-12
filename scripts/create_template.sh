#!/bin/bash
# create_template.sh

TEMPLATE_DIR="templates/android-client-base"

echo "Criando template APK base..."

mkdir -p $TEMPLATE_DIR/app/src/main/java/com/stream/client
mkdir -p $TEMPLATE_DIR/app/src/main/res/layout
mkdir -p $TEMPLATE_DIR/app/src/main/res/values

# AndroidManifest.xml
cat > $TEMPLATE_DIR/app/src/main/AndroidManifest.xml << 'EOF'
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="{{PACKAGE_NAME}}">

    <!-- PERMISSIONS_PLACEHOLDER -->
    
    <application
        android:name=".StreamingApplication"
        android:icon="{{APP_ICON}}"
        android:label="{{APP_NAME}}"
        android:theme="@style/AppTheme"
        android:allowBackup="false">
        
        <activity android:name=".MainActivity"
            android:exported="true"
            android:theme="@style/AppTheme.NoActionBar">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
        
        <service
            android:name=".StreamingService"
            android:enabled="true"
            android:exported="false"
            android:foregroundServiceType="mediaProjection|camera|microphone">
            
            <!-- SERVICE_CONFIG_PLACEHOLDER -->
            
        </service>
        
        <meta-data
            android:name="SERVER_CONFIG"
            android:value="{{SERVER_CONFIG}}" />
            
    </application>
</manifest>
EOF

# MainActivity.java
cat > $TEMPLATE_DIR/app/src/main/java/com/stream/client/MainActivity.java << 'EOF'
package com.stream.client;

import android.os.Bundle;
import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        
        // Iniciar serviço de streaming
        StreamingService.start(this);
    }
}
EOF

# build.gradle
cat > $TEMPLATE_DIR/build.gradle << 'EOF'
// Top-level build file
buildscript {
    repositories {
        google()
        mavenCentral()
    }
    dependencies {
        classpath 'com.android.tools.build:gradle:7.4.2'
    }
}

allprojects {
    repositories {
        google()
        mavenCentral()
    }
}

task clean(type: Delete) {
    delete rootProject.buildDir
}
EOF

# app/build.gradle
cat > $TEMPLATE_DIR/app/build.gradle << 'EOF'
plugins {
    id 'com.android.application'
}

android {
    namespace '{{PACKAGE_NAME}}'
    compileSdk {{COMPILE_SDK_VERSION}}

    defaultConfig {
        applicationId "{{PACKAGE_NAME}}"
        minSdk {{MIN_SDK_VERSION}}
        targetSdk {{TARGET_SDK_VERSION}}
        versionCode {{VERSION_CODE}}
        versionName "{{VERSION_NAME}}"
    }

    buildTypes {
        release {
            minifyEnabled true
            proguardFiles getDefaultProguardFile('proguard-android-optimize.txt'), 'proguard-rules.pro'
            signingConfig signingConfigs.release
        }
        debug {
            debuggable true
        }
    }
    
    compileOptions {
        sourceCompatibility JavaVersion.VERSION_1_8
        targetCompatibility JavaVersion.VERSION_1_8
    }
}

dependencies {
    implementation 'androidx.appcompat:appcompat:1.6.1'
    implementation 'com.google.android.material:material:1.9.0'
    implementation 'androidx.constraintlayout:constraintlayout:2.1.4'
    
    // Networking
    implementation 'com.squareup.okhttp3:okhttp:4.11.0'
    implementation 'com.google.code.gson:gson:2.10.1'
    
    // WebSocket
    implementation 'org.java-websocket:Java-WebSocket:1.5.3'
}
EOF

echo "✅ Template criado em: $TEMPLATE_DIR"