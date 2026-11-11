package com.worthdoingbadly.cheese

import java.util.Locale

/**
 * Interface text in English and Simplified Chinese.
 * Follows the system language, identical to the Mangekyo benchmark app.
 */
fun tr(en: String, zh: String): String =
    if (Locale.getDefault().language.startsWith("zh")) zh else en
