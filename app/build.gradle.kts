plugins {
    id("com.android.application")
}

android {
    namespace = "com.mystudio.editor"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.mystudio.editor"
        minSdk = 29            // Android 10, sama dengan Keyflow
        targetSdk = 36
        versionCode = 1
        versionName = "0.0.1"

        ndk {
            // Samsung S24 dan Honor Pad 10 sama-sama arm64; satu ABI = build lebih cepat, APK lebih kecil
            abiFilters += listOf("arm64-v8a")
        }
        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++20", "-O2", "-fno-exceptions", "-fno-rtti")
                arguments += listOf("-DANDROID_STL=c++_shared")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }
}
