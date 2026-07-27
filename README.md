# Enhanced Zora Swimming
## Part of a series of mods for [Dusklight](https://twilitrealm.dev/) attempting to modernize it

### The Problem
The Zora Armor in Twilight Princess is very cool conceptually, but a bit of a let-down in practice. It is, in my humble opinion, slow and boring. It's not to say that movement in Twilight Princess is incredible, but every other form of movement has a dash that gives a short burst of speed, which adds that tiny bit of interaction that moving across a flat surface kind of needs.

These problems are exacerbated by Dusklight, for two reasons. 
* **Full camera control via the right stick**: If your thumb is holding (A) to swim, it's not on the right stick to control the camera, which feels like a loss.
* **Unlocked Iron Boots movement speed**: Swimming was always slow across long distances, like Lake Hylia, but in dungeons, it was at least faster than the Iron Boots. Without that speed loss, there's no incentive to ever swim, as the Iron Boots are strictly better.

### My Solution
To understand everything this mod does, we need to define three distinct swim states- **Link Swim**, **Zora Swim**, and **Zora Dive**. 
* **Link Swim**: Link's default swimming mode, when he's wearing the Hero's Tunic or Magic Armor. You can swim at the surface, and tap (A) for a dash.
* **Zora Swim**: Link's base swimming mode in Zora armor. It's just like **Link Swim**, but with no dash. Instead, holding (A) transitions Link to **Zora Dive**
* **Zora Dive**: Link's full speed, 3-dimensional swimming while in Zora armor. While holding (A), Link swims forwards without any input, and he can go up or down as well as side to side.

With that in mind, this mod does the following:
1. Increases the base speed of **Zora Dive** to 1.6x vanilla.
2. Binds **Zora Dive** to (RT) instead of (A) and updates the HUD accordingly.
3. Adds a dash to **Zora Dive**, bound to (A).
4. Copies the dash from **Link Swim** to **Zora Swim**

The result is that swimming with the Zora Armor is faster, more engaging, and plays better to modern sensibilities. Most critically, moving **Zora Dive** to (RT) opens up the face buttons for use, allowing for future mods to add other features, like replicating the combat from Zora Link in Majora's Mask.

### Installation
Make sure you're on a version of Dusklight that supports mods- as of right now, you have to compile from source, but they should be available in the next numbered version. Once you've got that set up & have launched at least once, copy [enhanced_swimming.dusk](https://github.com/HadiSoufi/dusklight-enhanced_swimming/blob/main/enhanced_swimming.dusk) into `%APPDATA%\TwilitRealm\Dusklight\mods\`. It will then appear in the Mod menu & be enabled by default.
