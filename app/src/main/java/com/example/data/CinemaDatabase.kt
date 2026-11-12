package com.example.data

import android.content.Context
import androidx.room.Dao
import androidx.room.Database
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import androidx.room.Room
import androidx.room.RoomDatabase
import androidx.room.Update
import kotlinx.coroutines.flow.Flow

@Dao
interface CinemaDao {
    @Query("SELECT * FROM cine_clips ORDER BY createdAtEpochMs DESC")
    fun observeAllClips(): Flow<List<CineClipEntity>>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insertClip(clip: CineClipEntity): Long

    @Update
    suspend fun updateClip(clip: CineClipEntity)

    @Query("DELETE FROM cine_clips WHERE id = :id")
    suspend fun deleteClipById(id: Long)

    @Query("SELECT * FROM lut_presets ORDER BY id ASC")
    fun observeAllPresets(): Flow<List<LutPresetEntity>>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insertPreset(preset: LutPresetEntity): Long

    @Query("SELECT COUNT(*) FROM cine_clips")
    suspend fun getClipCount(): Int
}

@Database(
    entities = [CineClipEntity::class, LutPresetEntity::class],
    version = 1,
    exportSchema = false
)
abstract class CinemaDatabase : RoomDatabase() {
    abstract fun cinemaDao(): CinemaDao

    companion object {
        @Volatile
        private var INSTANCE: CinemaDatabase? = null

        fun getInstance(context: Context): CinemaDatabase {
            return INSTANCE ?: synchronized(this) {
                val instance = Room.databaseBuilder(
                    context.applicationContext,
                    CinemaDatabase::class.java,
                    "apexcine_workstation.db"
                ).build()
                INSTANCE = instance
                instance
            }
        }
    }
}

class CinemaRepository(private val dao: CinemaDao) {
    val allClips: Flow<List<CineClipEntity>> = dao.observeAllClips()
    val allPresets: Flow<List<LutPresetEntity>> = dao.observeAllPresets()

    suspend fun insertClip(clip: CineClipEntity): Long = dao.insertClip(clip)
    suspend fun updateClip(clip: CineClipEntity) = dao.updateClip(clip)
    suspend fun deleteClip(id: Long) = dao.deleteClipById(id)
    suspend fun insertPreset(preset: LutPresetEntity): Long = dao.insertPreset(preset)
    suspend fun getClipCount(): Int = dao.getClipCount()
}
